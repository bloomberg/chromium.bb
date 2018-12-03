// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BROWSER_MAIN_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BROWSER_MAIN_H_

#include <memory>

namespace content {
class BrowserMainRunner;
struct MainFunctionParams;
}  // namespace content

int WebTestBrowserMain(
    const content::MainFunctionParams& parameters,
    const std::unique_ptr<content::BrowserMainRunner>& main_runner);

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BROWSER_MAIN_H_
