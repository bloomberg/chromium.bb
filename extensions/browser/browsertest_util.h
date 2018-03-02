// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_
#define EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_

#include <string>

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
namespace browsertest_util {

// Waits until |script| calls "window.domAutomationController.send(result)",
// where |result| is a string, and returns |result|. Fails the test and returns
// an empty string if |extension_id| isn't installed in |context| or doesn't
// have a background page, or if executing the script fails.
std::string ExecuteScriptInBackgroundPage(content::BrowserContext* context,
                                          const std::string& extension_id,
                                          const std::string& script);

// Same as ExecuteScriptInBackgroundPage, but doesn't wait for the script
// to return a result. Fails the test and returns false if |extension_id|
// isn't installed in |context| or doesn't have a background page, or if
// executing the script fails.
bool ExecuteScriptInBackgroundPageNoWait(content::BrowserContext* context,
                                         const std::string& extension_id,
                                         const std::string& script);

}  // namespace browsertest_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_
