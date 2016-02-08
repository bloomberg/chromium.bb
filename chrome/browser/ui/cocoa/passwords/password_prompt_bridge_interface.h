// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_PROMPT_BRIDGE_INTERFACE_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_PROMPT_BRIDGE_INTERFACE_H_

class PasswordDialogController;
namespace net {
class URLRequestContextGetter;
}

// An interface for the bridge between AccountChooserViewController and platform
// independent UI code.
class PasswordPromptBridgeInterface {
 public:
  // Closes the dialog.
  virtual void PerformClose() = 0;

  // Returns the controller containing the data.
  virtual PasswordDialogController* GetDialogController() = 0;

  // Returns the request context for fetching the avatars.
  virtual net::URLRequestContextGetter* GetRequestContext() const = 0;

 protected:
  virtual ~PasswordPromptBridgeInterface() = default;
};

// A protocol implemented by the password dialog content view.
@protocol PasswordPromptViewInterface<NSObject>
// Returns the current bridge object.
@property(nonatomic) PasswordPromptBridgeInterface* bridge;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_PROMPT_BRIDGE_INTERFACE_H_
