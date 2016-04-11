// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_

#include <string>

#include "base/callback_forward.h"

namespace shell_integration {
namespace win {

// Initiates an OS shell flow which (if followed by the user) should set
// Chrome as the default browser. Returns false if the flow cannot be
// initialized, if it is not supported (introduced for Windows 8) or if the
// user cancels the operation. This is a blocking call and requires a FILE
// thread. If Chrome is already default browser, no interactive dialog will be
// shown and this method returns true.
bool SetAsDefaultBrowserUsingIntentPicker();

// Initiates the interaction with the system settings for the default browser.
// The function takes care of making sure |on_finished_callback| will get called
// exactly once when the interaction is finished.
void SetAsDefaultBrowserUsingSystemSettings(
    const base::Closure& on_finished_callback);

// Initiates an OS shell flow which (if followed by the user) should set
// Chrome as the default handler for |protocol|. Returns false if the flow
// cannot be initialized, if it is not supported (introduced for Windows 8)
// or if the user cancels the operation. This is a blocking call and requires
// a FILE thread. If Chrome is already default for |protocol|, no interactive
// dialog will be shown and this method returns true.
bool SetAsDefaultProtocolClientUsingIntentPicker(const std::string& protocol);

}  // namespace win
}  // namespace shell_integration

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_
