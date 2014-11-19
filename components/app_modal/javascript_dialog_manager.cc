// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_modal/javascript_dialog_manager.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "components/app_modal/app_modal_dialog.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_dialog_extensions_client.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "content/public/common/javascript_message_type.h"
#include "grit/components_strings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_modal {
namespace {

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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// JavaScriptDialogManager, public:

// static
JavaScriptDialogManager* JavaScriptDialogManager::GetInstance() {
  return Singleton<JavaScriptDialogManager>::get();
}

void JavaScriptDialogManager::SetNativeDialogFactory(
    scoped_ptr<JavaScriptNativeDialogFactory> factory) {
  native_dialog_factory_ = factory.Pass();
}

void JavaScriptDialogManager::SetExtensionsClient(
    scoped_ptr<JavaScriptDialogExtensionsClient> extensions_client) {
  extensions_client_ = extensions_client.Pass();
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
      &javascript_dialog_extra_data_[web_contents];

  if (extra_data->suppress_javascript_messages_) {
    *did_suppress_message = true;
    return;
  }

  base::TimeDelta time_since_last_message = base::TimeTicks::Now() -
      extra_data->last_javascript_message_dismissal_;
  bool display_suppress_checkbox = false;
  // If a WebContents is impolite and displays a second JavaScript
  // alert within kJavaScriptMessageExpectedDelay of a previous
  // JavaScript alert being dismissed, show a checkbox offering to
  // suppress future alerts from this WebContents.
  const int kJavaScriptMessageExpectedDelay = 1000;

  if (time_since_last_message <
      base::TimeDelta::FromMilliseconds(kJavaScriptMessageExpectedDelay)) {
    display_suppress_checkbox = true;
  } else {
    display_suppress_checkbox = false;
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
      display_suppress_checkbox,
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
      false,       // display_suppress_checkbox
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

void JavaScriptDialogManager::WebContentsDestroyed(
    content::WebContents* web_contents) {
  CancelActiveAndPendingDialogs(web_contents);
  javascript_dialog_extra_data_.erase(web_contents);
}

base::string16 JavaScriptDialogManager::GetTitle(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    bool is_alert) {
  // If the URL hasn't any host, return the default string.
  if (!origin_url.has_host()) {
      return l10n_util::GetStringUTF16(
          is_alert ? IDS_JAVASCRIPT_ALERT_DEFAULT_TITLE
                   : IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE);
  }

  // For extensions, show the extension name, but only if the origin of
  // the alert matches the top-level WebContents.
  std::string name;
  if (extensions_client_->GetExtensionName(web_contents, origin_url, &name))
    return base::UTF8ToUTF16(name);

  // Otherwise, return the formatted URL.
  // In this case, force URL to have LTR directionality.
  base::string16 url_string = net::FormatUrl(origin_url, accept_lang);
  return l10n_util::GetStringFUTF16(
      is_alert ? IDS_JAVASCRIPT_ALERT_TITLE
      : IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
      base::i18n::GetDisplayStringInLTRDirectionality(url_string));
}

void JavaScriptDialogManager::CancelActiveAndPendingDialogs(
    content::WebContents* web_contents) {
  AppModalDialogQueue* queue = AppModalDialogQueue::GetInstance();
  AppModalDialog* active_dialog = queue->active_dialog();
  if (active_dialog && active_dialog->web_contents() == web_contents)
    active_dialog->Invalidate();
  for (AppModalDialogQueue::iterator i = queue->begin();
       i != queue->end(); ++i) {
    if ((*i)->web_contents() == web_contents)
      (*i)->Invalidate();
  }
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

  callback.Run(success, user_input);
}

}  // namespace app_modal
