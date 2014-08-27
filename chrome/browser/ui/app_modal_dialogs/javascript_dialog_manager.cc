// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/javascript_message_type.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#endif  // defined(ENABLE_EXTENSIONS)

using content::BrowserContext;
using content::JavaScriptDialogManager;
using content::WebContents;

#if defined(ENABLE_EXTENSIONS)
using extensions::Extension;
#endif  // defined(ENABLE_EXTENSIONS)

namespace {

#if defined(ENABLE_EXTENSIONS)
// Returns the ProcessManager for the browser context from |web_contents|.
extensions::ProcessManager* GetExtensionsProcessManager(
    WebContents* web_contents) {
  return extensions::ExtensionSystem::Get(
      web_contents->GetBrowserContext())->process_manager();
}

// Returns the extension associated with |web_contents| or NULL if there is no
// associated extension (or extensions are not supported).
const Extension* GetExtensionForWebContents(WebContents* web_contents) {
  extensions::ProcessManager* pm = GetExtensionsProcessManager(web_contents);
  return pm->GetExtensionForRenderViewHost(web_contents->GetRenderViewHost());
}
#endif  // defined(ENABLE_EXTENSIONS)

// Keeps an |extension| from shutting down its lazy background page. If an
// extension opens a dialog its lazy background page must stay alive until the
// dialog closes.
void IncrementLazyKeepaliveCount(WebContents* web_contents) {
#if defined(ENABLE_EXTENSIONS)
  const Extension* extension = GetExtensionForWebContents(web_contents);
  if (extension == NULL)
    return;

  DCHECK(web_contents);
  extensions::ProcessManager* pm = GetExtensionsProcessManager(web_contents);
  if (pm)
    pm->IncrementLazyKeepaliveCount(extension);
#endif  // defined(ENABLE_EXTENSIONS)
}

// Allows an |extension| to shut down its lazy background page after a dialog
// closes (if nothing else is keeping it open).
void DecrementLazyKeepaliveCount(WebContents* web_contents) {
#if defined(ENABLE_EXTENSIONS)
  const Extension* extension = GetExtensionForWebContents(web_contents);
  if (extension == NULL)
    return;

  DCHECK(web_contents);
  extensions::ProcessManager* pm = GetExtensionsProcessManager(web_contents);
  if (pm)
    pm->DecrementLazyKeepaliveCount(extension);
#endif  // defined(ENABLE_EXTENSIONS)
}

class ChromeJavaScriptDialogManager : public JavaScriptDialogManager {
 public:
  static ChromeJavaScriptDialogManager* GetInstance();

  virtual void RunJavaScriptDialog(
      WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      content::JavaScriptMessageType message_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) OVERRIDE;

  virtual void RunBeforeUnloadDialog(
      WebContents* web_contents,
      const base::string16& message_text,
      bool is_reload,
      const DialogClosedCallback& callback) OVERRIDE;

  virtual bool HandleJavaScriptDialog(
      WebContents* web_contents,
      bool accept,
      const base::string16* prompt_override) OVERRIDE;

  virtual void CancelActiveAndPendingDialogs(
      WebContents* web_contents) OVERRIDE;

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ChromeJavaScriptDialogManager>;

  ChromeJavaScriptDialogManager();
  virtual ~ChromeJavaScriptDialogManager();

  base::string16 GetTitle(WebContents* web_contents,
                          const GURL& origin_url,
                          const std::string& accept_lang,
                          bool is_alert);

  // Wrapper around a DialogClosedCallback so that we can intercept it before
  // passing it onto the original callback.
  void OnDialogClosed(WebContents* web_contents,
                      DialogClosedCallback callback,
                      bool success,
                      const base::string16& user_input);

  // Mapping between the WebContents and their extra data. The key
  // is a void* because the pointer is just a cookie and is never dereferenced.
  JavaScriptAppModalDialog::ExtraDataMap javascript_dialog_extra_data_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptDialogManager);
};

////////////////////////////////////////////////////////////////////////////////
// ChromeJavaScriptDialogManager, public:

ChromeJavaScriptDialogManager::ChromeJavaScriptDialogManager() {
}

ChromeJavaScriptDialogManager::~ChromeJavaScriptDialogManager() {
}

// static
ChromeJavaScriptDialogManager* ChromeJavaScriptDialogManager::GetInstance() {
  return Singleton<ChromeJavaScriptDialogManager>::get();
}

void ChromeJavaScriptDialogManager::RunJavaScriptDialog(
    WebContents* web_contents,
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
  // Show a checkbox offering to suppress further messages if this message is
  // being displayed within kJavaScriptMessageExpectedDelay of the last one.
  if (time_since_last_message <
      base::TimeDelta::FromMilliseconds(
          chrome::kJavaScriptMessageExpectedDelay)) {
    display_suppress_checkbox = true;
  } else {
    display_suppress_checkbox = false;
  }

  bool is_alert = message_type == content::JAVASCRIPT_MESSAGE_TYPE_ALERT;
  base::string16 dialog_title =
      GetTitle(web_contents, origin_url, accept_lang, is_alert);

  IncrementLazyKeepaliveCount(web_contents);

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
      base::Bind(&ChromeJavaScriptDialogManager::OnDialogClosed,
                 base::Unretained(this), web_contents, callback)));
}

void ChromeJavaScriptDialogManager::RunBeforeUnloadDialog(
    WebContents* web_contents,
    const base::string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  const base::string16 title = l10n_util::GetStringUTF16(is_reload ?
      IDS_BEFORERELOAD_MESSAGEBOX_TITLE : IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE);
  const base::string16 footer = l10n_util::GetStringUTF16(is_reload ?
      IDS_BEFORERELOAD_MESSAGEBOX_FOOTER : IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);

  base::string16 full_message =
      message_text + base::ASCIIToUTF16("\n\n") + footer;

  IncrementLazyKeepaliveCount(web_contents);

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
      base::Bind(&ChromeJavaScriptDialogManager::OnDialogClosed,
                 base::Unretained(this), web_contents, callback)));
}

bool ChromeJavaScriptDialogManager::HandleJavaScriptDialog(
    WebContents* web_contents,
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

void ChromeJavaScriptDialogManager::WebContentsDestroyed(
    WebContents* web_contents) {
  CancelActiveAndPendingDialogs(web_contents);
  javascript_dialog_extra_data_.erase(web_contents);
}

base::string16 ChromeJavaScriptDialogManager::GetTitle(
    WebContents* web_contents,
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
#if defined(ENABLE_EXTENSIONS)
  const Extension* extension = GetExtensionForWebContents(web_contents);
  if (extension &&
      web_contents->GetLastCommittedURL().GetOrigin() == origin_url) {
    return base::UTF8ToUTF16(extension->name());
  }
#endif  // defined(ENABLE_EXTENSIONS)

  // Otherwise, return the formatted URL.
  // In this case, force URL to have LTR directionality.
  base::string16 url_string = net::FormatUrl(origin_url, accept_lang);
  return l10n_util::GetStringFUTF16(
      is_alert ? IDS_JAVASCRIPT_ALERT_TITLE
      : IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
      base::i18n::GetDisplayStringInLTRDirectionality(url_string));
}

void ChromeJavaScriptDialogManager::CancelActiveAndPendingDialogs(
    WebContents* web_contents) {
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

void ChromeJavaScriptDialogManager::OnDialogClosed(
    WebContents* web_contents,
    DialogClosedCallback callback,
    bool success,
    const base::string16& user_input) {
  // If an extension opened this dialog then the extension may shut down its
  // lazy background page after the dialog closes. (Dialogs are closed before
  // their WebContents is destroyed so |web_contents| is still valid here.)
  DecrementLazyKeepaliveCount(web_contents);

  callback.Run(success, user_input);
}

}  // namespace

content::JavaScriptDialogManager* GetJavaScriptDialogManagerInstance() {
  return ChromeJavaScriptDialogManager::GetInstance();
}
