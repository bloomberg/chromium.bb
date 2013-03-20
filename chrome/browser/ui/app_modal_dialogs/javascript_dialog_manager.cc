// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"

#include <map>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/singleton.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/javascript_message_type.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using content::JavaScriptDialogManager;
using content::WebContents;

namespace {

class ChromeJavaScriptDialogManager : public JavaScriptDialogManager,
                                      public content::NotificationObserver {
 public:
  static ChromeJavaScriptDialogManager* GetInstance();

  explicit ChromeJavaScriptDialogManager(
      extensions::ExtensionHost* extension_host);
  virtual ~ChromeJavaScriptDialogManager();

  virtual void RunJavaScriptDialog(
      WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      content::JavaScriptMessageType message_type,
      const string16& message_text,
      const string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) OVERRIDE;

  virtual void RunBeforeUnloadDialog(
      WebContents* web_contents,
      const string16& message_text,
      bool is_reload,
      const DialogClosedCallback& callback) OVERRIDE;

  virtual bool HandleJavaScriptDialog(
      WebContents* web_contents,
      bool accept,
      const string16* prompt_override) OVERRIDE;

  virtual void ResetJavaScriptState(WebContents* web_contents) OVERRIDE;

 private:
  ChromeJavaScriptDialogManager();

  friend struct DefaultSingletonTraits<ChromeJavaScriptDialogManager>;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  string16 GetTitle(const GURL& origin_url,
                    const std::string& accept_lang,
                    bool is_alert);

  void CancelPendingDialogs(WebContents* web_contents);

  // Wrapper around a DialogClosedCallback so that we can intercept it before
  // passing it onto the original callback.
  void OnDialogClosed(DialogClosedCallback callback,
                      bool success,
                      const string16& user_input);

  // Mapping between the WebContents and their extra data. The key
  // is a void* because the pointer is just a cookie and is never dereferenced.
  typedef std::map<void*, ChromeJavaScriptDialogExtraData>
      JavaScriptDialogExtraDataMap;
  JavaScriptDialogExtraDataMap javascript_dialog_extra_data_;

  // Extension Host which owns the ChromeJavaScriptDialogManager instance.
  // It's used to get a extension name from a URL.
  // If it's not owned by any Extension, it should be NULL.
  extensions::ExtensionHost* extension_host_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptDialogManager);
};

////////////////////////////////////////////////////////////////////////////////
// ChromeJavaScriptDialogManager, public:

ChromeJavaScriptDialogManager::ChromeJavaScriptDialogManager()
    : extension_host_(NULL) {
}

ChromeJavaScriptDialogManager::~ChromeJavaScriptDialogManager() {
  extension_host_ = NULL;
}

ChromeJavaScriptDialogManager::ChromeJavaScriptDialogManager(
    extensions::ExtensionHost* extension_host)
    : extension_host_(extension_host) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<Profile>(extension_host_->profile()));
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
    const string16& message_text,
    const string16& default_prompt_text,
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
  // being displayed within kJavascriptMessageExpectedDelay of the last one.
  if (time_since_last_message <
      base::TimeDelta::FromMilliseconds(
          chrome::kJavascriptMessageExpectedDelay)) {
    display_suppress_checkbox = true;
  }

  bool is_alert = message_type == content::JAVASCRIPT_MESSAGE_TYPE_ALERT;
  string16 dialog_title = GetTitle(origin_url, accept_lang, is_alert);

  if (extension_host_)
    extension_host_->WillRunJavaScriptDialog();

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      web_contents,
      extra_data,
      dialog_title,
      message_type,
      message_text,
      default_prompt_text,
      display_suppress_checkbox,
      false,  // is_before_unload_dialog
      false,  // is_reload
      base::Bind(&ChromeJavaScriptDialogManager::OnDialogClosed,
                 base::Unretained(this), callback)));
}

void ChromeJavaScriptDialogManager::RunBeforeUnloadDialog(
    WebContents* web_contents,
    const string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_[web_contents];

  const string16 title = l10n_util::GetStringUTF16(is_reload ?
      IDS_BEFORERELOAD_MESSAGEBOX_TITLE : IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE);
  const string16 footer = l10n_util::GetStringUTF16(is_reload ?
      IDS_BEFORERELOAD_MESSAGEBOX_FOOTER : IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);

  string16 full_message = message_text + ASCIIToUTF16("\n\n") + footer;

  if (extension_host_)
    extension_host_->WillRunJavaScriptDialog();

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      web_contents,
      extra_data,
      title,
      content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
      full_message,
      string16(),  // default_prompt_text
      false,       // display_suppress_checkbox
      true,        // is_before_unload_dialog
      is_reload,
      base::Bind(&ChromeJavaScriptDialogManager::OnDialogClosed,
                 base::Unretained(this), callback)));
}

bool ChromeJavaScriptDialogManager::HandleJavaScriptDialog(
    WebContents* web_contents,
    bool accept,
    const string16* prompt_override) {
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

void ChromeJavaScriptDialogManager::ResetJavaScriptState(
    WebContents* web_contents) {
  CancelPendingDialogs(web_contents);
  javascript_dialog_extra_data_.erase(web_contents);
}

void ChromeJavaScriptDialogManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED);
  extension_host_ = NULL;
}

string16 ChromeJavaScriptDialogManager::GetTitle(const GURL& origin_url,
                                                 const std::string& accept_lang,
                                                 bool is_alert) {
  // If the URL hasn't any host, return the default string.
  if (!origin_url.has_host()) {
      return l10n_util::GetStringUTF16(
          is_alert ? IDS_JAVASCRIPT_ALERT_DEFAULT_TITLE
                   : IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE);
  }

  // If the URL is a chrome extension one, return the extension name.
  if (extension_host_) {
    const extensions::Extension* extension = extension_host_->
      profile()->GetExtensionService()->extensions()->
      GetExtensionOrAppByURL(ExtensionURLInfo(origin_url));
    if (extension) {
      return UTF8ToUTF16(base::StringPiece(extension->name()));
    }
  }

  // Otherwise, return the formatted URL.
  // In this case, force URL to have LTR directionality.
  string16 url_string = net::FormatUrl(origin_url, accept_lang);
  return l10n_util::GetStringFUTF16(
      is_alert ? IDS_JAVASCRIPT_ALERT_TITLE
      : IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
      base::i18n::GetDisplayStringInLTRDirectionality(url_string));
}

void ChromeJavaScriptDialogManager::CancelPendingDialogs(
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
    DialogClosedCallback callback,
    bool success,
    const string16& user_input) {
  if (extension_host_)
    extension_host_->DidCloseJavaScriptDialog();
  callback.Run(success, user_input);
}

}  // namespace

content::JavaScriptDialogManager* GetJavaScriptDialogManagerInstance() {
  return ChromeJavaScriptDialogManager::GetInstance();
}

content::JavaScriptDialogManager* CreateJavaScriptDialogManagerInstance(
    extensions::ExtensionHost* extension_host) {
  return new ChromeJavaScriptDialogManager(extension_host);
}
