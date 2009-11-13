// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JSMESSAGE_BOX_CLIENT_H_
#define CHROME_BROWSER_JSMESSAGE_BOX_CLIENT_H_

// JavaScriptMessageBoxClient
//
// An interface implemented by an object that receives results from JavaScript
// message boxes (alert, confirm, and prompt).
//

#include <string>

#include "app/gfx/native_widget_types.h"

class GURL;
class Profile;
class TabContents;
namespace IPC {
class Message;
}

class JavaScriptMessageBoxClient {
 public:
  virtual ~JavaScriptMessageBoxClient() {}

  // Returns the title to use for the message box.
  virtual std::wstring GetMessageBoxTitle(const GURL& frame_url,
                                          bool is_alert) = 0;

  // Returns the root native window with which the message box is associated.
  virtual gfx::NativeWindow GetMessageBoxRootWindow() = 0;

  // AppModalDialog calls this when the dialog is closed.
  virtual void OnMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt) = 0;

  // Indicates whether additional message boxes should be suppressed.
  virtual void SetSuppressMessageBoxes(bool suppress_message_boxes) = 0;

  // Returns the TabContents associated with this message box -- in practice,
  // the TabContents implementing this interface -- or NULL if it has no
  // TabContents (e.g., it's an ExtensionHost).
  virtual TabContents* AsTabContents() = 0;
};

#endif  // CHROME_BROWSER_JSMESSAGE_BOX_CLIENT_H_
