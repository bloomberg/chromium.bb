// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/utf_string_conversions.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "content/browser/javascript_dialogs.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"

class ChromeJavaScriptDialogCreator : public content::JavaScriptDialogCreator {
 public:
  static ChromeJavaScriptDialogCreator* GetInstance();

  virtual void RunJavaScriptDialog(content::JavaScriptDialogDelegate* delegate,
                                   TitleType title_type,
                                   const string16& title,
                                   int dialog_flags,
                                   const string16& message_text,
                                   const string16& default_prompt_text,
                                   IPC::Message* reply_message,
                                   bool* did_suppress_message) OVERRIDE;

  virtual void RunBeforeUnloadDialog(
      content::JavaScriptDialogDelegate* delegate,
      const string16& message_text,
      IPC::Message* reply_message) OVERRIDE;

  virtual void ResetJavaScriptState(
      content::JavaScriptDialogDelegate* delegate) OVERRIDE;

 private:
  explicit ChromeJavaScriptDialogCreator();
  virtual ~ChromeJavaScriptDialogCreator();

  friend struct DefaultSingletonTraits<ChromeJavaScriptDialogCreator>;

  string16 GetTitle(TitleType title_type,
                    const string16& title,
                    bool is_alert);

  void CancelPendingDialogs(content::DialogDelegate* delegate);

  // Mapping between the JavaScriptDialogDelegates and their extra data. The key
  // is a void* because the pointer is just a cookie and is never dereferenced.
  typedef std::map<void*, ChromeJavaScriptDialogExtraData>
      JavaScriptDialogExtraDataMap;
  JavaScriptDialogExtraDataMap javascript_dialog_extra_data_;
};

//------------------------------------------------------------------------------

ChromeJavaScriptDialogCreator::ChromeJavaScriptDialogCreator() {
}

ChromeJavaScriptDialogCreator::~ChromeJavaScriptDialogCreator() {
}

/* static */
ChromeJavaScriptDialogCreator* ChromeJavaScriptDialogCreator::GetInstance() {
  return Singleton<ChromeJavaScriptDialogCreator>::get();
}

void ChromeJavaScriptDialogCreator::RunJavaScriptDialog(
    content::JavaScriptDialogDelegate* delegate,
    TitleType title_type,
    const string16& title,
    int dialog_flags,
    const string16& message_text,
    const string16& default_prompt_text,
    IPC::Message* reply_message,
    bool* did_suppress_message)  {
  *did_suppress_message = false;

  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_[delegate];

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

  bool is_alert = dialog_flags == ui::MessageBoxFlags::kIsJavascriptAlert;
  string16 dialog_title = GetTitle(title_type, title, is_alert);

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      delegate,
      extra_data,
      dialog_title,
      dialog_flags,
      message_text,
      default_prompt_text,
      display_suppress_checkbox,
      false,  // is_before_unload_dialog
      reply_message));
}

void ChromeJavaScriptDialogCreator::RunBeforeUnloadDialog(
    content::JavaScriptDialogDelegate* delegate,
    const string16& message_text,
    IPC::Message* reply_message) {
  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_[delegate];

  string16 full_message = message_text + ASCIIToUTF16("\n\n") +
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      delegate,
      extra_data,
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE),
      ui::MessageBoxFlags::kIsJavascriptConfirm,
      full_message,
      string16(),  // default_prompt_text
      false,       // display_suppress_checkbox
      true,        // is_before_unload_dialog
      reply_message));
}

void ChromeJavaScriptDialogCreator::ResetJavaScriptState(
    content::JavaScriptDialogDelegate* delegate) {
  CancelPendingDialogs(delegate);
  javascript_dialog_extra_data_.erase(delegate);
}

string16 ChromeJavaScriptDialogCreator::GetTitle(TitleType title_type,
                                                 const string16& title,
                                                 bool is_alert) {
  switch (title_type) {
    case DIALOG_TITLE_NONE: {
      return l10n_util::GetStringUTF16(
          is_alert ? IDS_JAVASCRIPT_ALERT_DEFAULT_TITLE
                   : IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE);
      break;
    }
    case DIALOG_TITLE_PLAIN_STRING: {
      return title;
      break;
    }
    case DIALOG_TITLE_FORMATTED_URL: {
      // Force URL to have LTR directionality.
      return l10n_util::GetStringFUTF16(
          is_alert ? IDS_JAVASCRIPT_ALERT_TITLE
                   : IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
          base::i18n::GetDisplayStringInLTRDirectionality(title));
      break;
    }
  }
  NOTREACHED();
  return string16();
}

void ChromeJavaScriptDialogCreator::CancelPendingDialogs(
    content::DialogDelegate* delegate) {
  AppModalDialogQueue* queue = AppModalDialogQueue::GetInstance();
  AppModalDialog* active_dialog = queue->active_dialog();
  if (active_dialog && active_dialog->delegate() == delegate)
    active_dialog->Invalidate();
  for (AppModalDialogQueue::iterator i = queue->begin();
       i != queue->end(); ++i) {
    if ((*i)->delegate() == delegate)
      (*i)->Invalidate();
  }
}

//------------------------------------------------------------------------------

content::JavaScriptDialogCreator* GetJavaScriptDialogCreatorInstance() {
  return ChromeJavaScriptDialogCreator::GetInstance();
}
