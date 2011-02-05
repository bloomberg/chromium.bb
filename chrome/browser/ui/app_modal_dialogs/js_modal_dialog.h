// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JS_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JS_MODAL_DIALOG_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class ExtensionHost;
class NativeAppModalDialog;
class TabContents;

namespace IPC {
class Message;
}

class JavaScriptAppModalDialogDelegate {
 public:
  // AppModalDialog calls this when the dialog is closed.
  virtual void OnMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt) = 0;

  // Indicates whether additional message boxes should be suppressed.
  virtual void SetSuppressMessageBoxes(bool suppress_message_boxes) = 0;

  // Returns the root native window with which the message box is associated.
  virtual gfx::NativeWindow GetMessageBoxRootWindow() = 0;

  // Returns the TabContents or ExtensionHost associated with this message
  // box -- in practice, the object implementing this interface. Exactly one
  // of these must be non-NULL; behavior is undefined (read: it'll probably
  // crash) if that is not the case.
  virtual TabContents* AsTabContents() = 0;
  virtual ExtensionHost* AsExtensionHost() = 0;

 protected:
  virtual ~JavaScriptAppModalDialogDelegate() {}
};

// A controller + model class for JavaScript alert, confirm, prompt, and
// onbeforeunload dialog boxes.
class JavaScriptAppModalDialog : public AppModalDialog,
                                 public NotificationObserver {
 public:
  JavaScriptAppModalDialog(JavaScriptAppModalDialogDelegate* delegate,
                           const std::wstring& title,
                           int dialog_flags,
                           const std::wstring& message_text,
                           const std::wstring& default_prompt_text,
                           bool display_suppress_checkbox,
                           bool is_before_unload_dialog,
                           IPC::Message* reply_msg);
  virtual ~JavaScriptAppModalDialog();

  // Overridden from AppModalDialog:
  virtual NativeAppModalDialog* CreateNativeDialog();

  JavaScriptAppModalDialogDelegate* delegate() const { return delegate_; }

  // Callbacks from NativeDialog when the user accepts or cancels the dialog.
  void OnCancel(bool suppress_js_messages);
  void OnAccept(const std::wstring& prompt_text, bool suppress_js_messages);

  // NOTE: This is only called under Views, and should be removed. Any critical
  // work should be done in OnCancel or OnAccept. See crbug.com/63732 for more.
  void OnClose();

  // Accessors
  int dialog_flags() const { return dialog_flags_; }
  std::wstring message_text() const { return message_text_; }
  std::wstring default_prompt_text() const { return default_prompt_text_; }
  bool display_suppress_checkbox() const { return display_suppress_checkbox_; }
  bool is_before_unload_dialog() const { return is_before_unload_dialog_; }

 private:
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Initializes for notifications to listen.
  void InitNotifications();

  // Notifies the delegate with the result of the dialog.
  void NotifyDelegate(bool success, const std::wstring& prompt_text,
                      bool suppress_js_messages);

  NotificationRegistrar registrar_;

  // An implementation of the client interface to provide supporting methods
  // and receive results.
  JavaScriptAppModalDialogDelegate* delegate_;

  // The client_ as an ExtensionHost, cached for use during notifications that
  // may arrive after the client has entered its destructor (and is thus
  // treated as a base Delegate). This will be NULL if the |delegate_| is not an
  // ExtensionHost.
  ExtensionHost* extension_host_;

  // Information about the message box is held in the following variables.
  int dialog_flags_;
  std::wstring message_text_;
  std::wstring default_prompt_text_;
  bool display_suppress_checkbox_;
  bool is_before_unload_dialog_;
  IPC::Message* reply_msg_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialog);
};

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JS_MODAL_DIALOG_H_
