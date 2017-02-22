// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_

#include "build/build_config.h"
#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace test_runner {

class MockWebThemeEngine : public blink::WebThemeEngine {
 public:
  virtual ~MockWebThemeEngine() {}

#if !defined(OS_MACOSX)
  // blink::WebThemeEngine:
  blink::WebSize getSize(blink::WebThemeEngine::Part) override;
  void paint(blink::WebCanvas*,
             blink::WebThemeEngine::Part,
             blink::WebThemeEngine::State,
             const blink::WebRect&,
             const blink::WebThemeEngine::ExtraParams*) override;
#endif  // !defined(OS_MACOSX)
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
