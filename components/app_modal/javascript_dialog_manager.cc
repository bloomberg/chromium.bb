// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_modal/javascript_dialog_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/app_modal/app_modal_dialog.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/javascript_dialog_extensions_client.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/javascript_message_type.h"
#include "grit/components_strings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"

namespace app_modal {
namespace {

#if !defined(OS_ANDROID)
// Keep in sync with kDefaultMessageWidth, but allow some space for the rest of
// the text.
const int kUrlElideWidth = 350;
#endif

class DefaultExtensionsClient : public JavaScriptDialogExtensionsClient {
 public:
  DefaultExtensionsClient() {}
  ~DefaultExtensionsClient() override {}

 private:
  // JavaScriptDialogExtensionsClient:
  void OnDialogOpened(content::WebContents* web_contents) override {}
  void OnDialogClosed(content::WebContents* web_contents) override {}
  bool GetExtensionName(content::WebContents* web_contents,
                        const GURL& origin_url,
                        std::string* name_out) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultExtensionsClient);
};

bool ShouldDisplaySuppressCheckbox(
    ChromeJavaScriptDialogExtraData* extra_data) {
  return extra_data->has_already_shown_a_dialog_;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// JavaScriptDialogManager, public:

// static
JavaScriptDialogManager* JavaScriptDialogManager::GetInstance() {
  return base::Singleton<JavaScriptDialogManager>::get();
}

void JavaScriptDialogManager::SetNativeDialogFactory(
    scoped_ptr<JavaScriptNativeDialogFactory> factory) {
  native_dialog_factory_ = std::move(factory);
}

void JavaScriptDialogManager::SetExtensionsClient(
    scoped_ptr<JavaScriptDialogExtensionsClient> extensions_client) {
  extensions_client_ = std::move(extensions_client);
}

////////////////////////////////////////////////////////////////////////////////
// JavaScriptDialogManager, private:

JavaScriptDialogManager::JavaScriptDialogManager()
    : extensions_client_(new DefaultExtensionsClient) {
}

JavaScriptDialogManager::~JavaScriptDialogManager() {
}

void JavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    content::JavaScriptMessageType message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message)  {
  *did_suppress_message = false;

  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_
          [JavaScriptAppModalDialog::GetSerializedOriginForWebContents(
              web_contents)];

  if (extra_data->suppress_javascript_messages_) {
    *did_suppress_message = true;
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_creation_time_.is_null()) {
    // A new dialog has been created: log the time since the last one was
    // created.
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "JSDialogs.FineTiming.TimeBetweenDialogCreatedAndNextDialogCreated",
        now - last_creation_time_);
  }
  last_creation_time_ = now;

  // Also log the time since a dialog was closed, but only if this is the first
  // dialog that was opened since the closing.
  if (!last_close_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "JSDialogs.FineTiming.TimeBetweenDialogClosedAndNextDialogCreated",
        now - last_close_time_);
    last_close_time_ = base::TimeTicks();
  }

  bool is_alert = message_type == content::JAVASCRIPT_MESSAGE_TYPE_ALERT;
  base::string16 dialog_title =
      GetTitle(web_contents, origin_url, accept_lang, is_alert);

  extensions_client_->OnDialogOpened(web_contents);

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      web_contents,
      &javascript_dialog_extra_data_,
      dialog_title,
      message_type,
      message_text,
      default_prompt_text,
      ShouldDisplaySuppressCheckbox(extra_data),
      false,  // is_before_unload_dialog
      false,  // is_reload
      base::Bind(&JavaScriptDialogManager::OnDialogClosed,
                 base::Unretained(this), web_contents, callback)));
}

void JavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    const base::string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_
          [JavaScriptAppModalDialog::GetSerializedOriginForWebContents(
              web_contents)];

  if (extra_data->suppress_javascript_messages_) {
    // If a site harassed the user enough for them to put it on mute, then it
    // lost its privilege to deny unloading.
    callback.Run(true, base::string16());
    return;
  }

  const base::string16 title = l10n_util::GetStringUTF16(is_reload ?
      IDS_BEFORERELOAD_MESSAGEBOX_TITLE : IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE);
  const base::string16 footer = l10n_util::GetStringUTF16(is_reload ?
      IDS_BEFORERELOAD_MESSAGEBOX_FOOTER : IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);

  base::string16 full_message =
      message_text + base::ASCIIToUTF16("\n\n") + footer;

  extensions_client_->OnDialogOpened(web_contents);

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      web_contents,
      &javascript_dialog_extra_data_,
      title,
      content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
      full_message,
      base::string16(),  // default_prompt_text
      ShouldDisplaySuppressCheckbox(extra_data),
      true,        // is_before_unload_dialog
      is_reload,
      base::Bind(&JavaScriptDialogManager::OnDialogClosed,
                 base::Unretained(this), web_contents, callback)));
}

bool JavaScriptDialogManager::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const base::string16* prompt_override) {
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  if (!dialog_queue->HasActiveDialog() ||
      !dialog_queue->active_dialog()->IsJavaScriptModalDialog() ||
      dialog_queue->active_dialog()->web_contents() != web_contents) {
    return false;
  }
  JavaScriptAppModalDialog* dialog = static_cast<JavaScriptAppModalDialog*>(
      dialog_queue->active_dialog());
  if (accept) {
    if (prompt_override)
      dialog->SetOverridePromptText(*prompt_override);
    dialog->native_dialog()->AcceptAppModalDialog();
  } else {
    dialog->native_dialog()->CancelAppModalDialog();
  }
  return true;
}

void JavaScriptDialogManager::ResetDialogState(
    content::WebContents* web_contents) {
  CancelActiveAndPendingDialogs(web_contents);
  javascript_dialog_extra_data_.erase(
      JavaScriptAppModalDialog::GetSerializedOriginForWebContents(
          web_contents));
}

base::string16 JavaScriptDialogManager::GetTitle(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    bool is_alert) {
  // For extensions, show the extension name, but only if the origin of
  // the alert matches the top-level WebContents.
  std::string name;
  if (extensions_client_->GetExtensionName(web_contents, origin_url, &name))
    return base::UTF8ToUTF16(name);

  // Otherwise, return the formatted URL. For non-standard URLs such as |data:|,
  // just say "This page".
  bool is_same_origin_as_main_frame =
      (web_contents->GetURL().GetOrigin() == origin_url.GetOrigin());
  if (origin_url.IsStandard() && !origin_url.SchemeIsFile() &&
      !origin_url.SchemeIsFileSystem()) {
#if !defined(OS_ANDROID)
    base::string16 url_string =
        url_formatter::ElideHost(origin_url, gfx::FontList(), kUrlElideWidth);
#else
    base::string16 url_string =
        url_formatter::FormatUrlForSecurityDisplayOmitScheme(origin_url,
                                                             accept_lang);
#endif
    return l10n_util::GetStringFUTF16(
        is_same_origin_as_main_frame ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE
                                     : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_IFRAME,
        base::i18n::GetDisplayStringInLTRDirectionality(url_string));
  }
  return l10n_util::GetStringUTF16(
      is_same_origin_as_main_frame
          ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL
          : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
}

void JavaScriptDialogManager::CancelActiveAndPendingDialogs(
    content::WebContents* web_contents) {
  AppModalDialogQueue* queue = AppModalDialogQueue::GetInstance();
  AppModalDialog* active_dialog = queue->active_dialog();
  for (AppModalDialogQueue::iterator i = queue->begin();
       i != queue->end(); ++i) {
    // Invalidating the active dialog might trigger showing a not-yet
    // invalidated dialog, so invalidate the active dialog last.
    if ((*i) == active_dialog)
      continue;
    if ((*i)->web_contents() == web_contents)
      (*i)->Invalidate();
  }
  if (active_dialog && active_dialog->web_contents() == web_contents)
    active_dialog->Invalidate();
}

void JavaScriptDialogManager::OnDialogClosed(
    content::WebContents* web_contents,
    DialogClosedCallback callback,
    bool success,
    const base::string16& user_input) {
  // If an extension opened this dialog then the extension may shut down its
  // lazy background page after the dialog closes. (Dialogs are closed before
  // their WebContents is destroyed so |web_contents| is still valid here.)
  extensions_client_->OnDialogClosed(web_contents);

  last_close_time_ = base::TimeTicks::Now();

  callback.Run(success, user_input);
}

}  // namespace app_modal
