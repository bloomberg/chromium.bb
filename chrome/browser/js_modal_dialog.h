// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JS_MODAL_DIALOG_H_
#define CHROME_BROWSER_JS_MODAL_DIALOG_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class ExtensionHost;
class JavaScriptMessageBoxClient;
class NativeAppModalDialog;

namespace IPC {
class Message;
}

// A controller+model class for JavaScript alert, confirm, prompt, and
// onbeforeunload dialog boxes.
class JavaScriptAppModalDialog : public AppModalDialog,
                                 public NotificationObserver {
 public:
  // A union of data necessary to determine the type of message box to
  // show.  |dialog_flags| is a MessageBox flag.
  JavaScriptAppModalDialog(JavaScriptMessageBoxClient* client,
                           const std::wstring& title,
                           int dialog_flags,
                           const std::wstring& message_text,
                           const std::wstring& default_prompt_text,
                           bool display_suppress_checkbox,
                           bool is_before_unload_dialog,
                           IPC::Message* reply_msg);
  virtual ~JavaScriptAppModalDialog();

  // AppModalDialog overrides.
  virtual NativeAppModalDialog* CreateNativeDialog();

  /////////////////////////////////////////////////////////////////////////////
  // Getters so NativeDialog can get information about the message box.
  JavaScriptMessageBoxClient* client() const { return client_; }

  // Callbacks from NativeDialog when the user accepts or cancels the dialog.
  void OnCancel();
  void OnAccept(const std::wstring& prompt_text, bool suppress_js_messages);
  void OnClose();

  // Accessors
  int dialog_flags() const { return dialog_flags_; }
  std::wstring message_text() const { return message_text_; }
  std::wstring default_prompt_text() const { return default_prompt_text_; }
  bool display_suppress_checkbox() const { return display_suppress_checkbox_; }
  bool is_before_unload_dialog() const { return is_before_unload_dialog_; }

 protected:
  // AppModalDialog overrides.
  virtual void Cleanup();

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Initializes for notifications to listen.
  void InitNotifications();

  NativeAppModalDialog* native_dialog_;

  NotificationRegistrar registrar_;

  // An implementation of the client interface to provide supporting methods
  // and receive results.
  JavaScriptMessageBoxClient* client_;

  // The client_ as an ExtensionHost, cached for use during notifications that
  // may arrive after the client has entered its destructor (and is thus
  // treated as a base JavaScriptMessageBoxClient). This will be NULL if the
  // client is not an ExtensionHost.
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

#endif  // CHROME_BROWSER_JS_MODAL_DIALOG_H_
