// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_COMMON_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_COMMON_H_

#include <string>

namespace content {
class WebContents;
}

// This function will execute the provided |javascript| until it causes a call
// to window.domAutomationController.send() with |evaluates_to| as the message.
// That is, we are NOT checking what the javascript evaluates to. Returns false
// if we exceed the TestTimeouts::action_max_timeout().
// TODO(phoglund): Consider a better interaction method with the javascript
// than polling javascript methods.
bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents);
bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents,
                      int poll_interval_msec);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_COMMON_H_
