// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements a browser-side endpoint for ChromeFrame UI automation
// activity. The client-side endpoint is implemented by
// ChromeFrameAutomationClient.
// The entire lifetime of this object should be contained within that of
// the BrowserProcess

#ifndef CHROME_BROWSER_AUTOMATION_CHROME_FRAME_AUTOMATION_PROVIDER_WIN_H_
#define CHROME_BROWSER_AUTOMATION_CHROME_FRAME_AUTOMATION_PROVIDER_WIN_H_

#include "base/basictypes.h"
#include "chrome/browser/automation/automation_provider.h"

class Profile;

// This class services automation IPC requests coming in from ChromeFrame
// instances.
class ChromeFrameAutomationProvider : public AutomationProvider {
 public:
  explicit ChromeFrameAutomationProvider(Profile* profile);
  virtual ~ChromeFrameAutomationProvider();

  // IPC::Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

 protected:
  // This function is called when we receive an invalid message type.
  virtual void OnUnhandledMessage(const IPC::Message& message);

  // Returns true if the message received is a valid chrome frame message.
  bool IsValidMessage(uint32 type);

  // Called to release an instance's ref count on the global BrowserProcess
  // instance.
  static void ReleaseBrowserProcess();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeFrameAutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_CHROME_FRAME_AUTOMATION_PROVIDER_WIN_H_

