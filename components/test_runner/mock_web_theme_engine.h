// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_

#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace test_runner {

class MockWebThemeEngine : public blink::WebThemeEngine {
 public:
  virtual ~MockWebThemeEngine() {}

#if !defined(OS_MACOSX)
  // blink::WebThemeEngine:
  virtual blink::WebSize getSize(blink::WebThemeEngine::Part);
  virtual void paint(blink::WebCanvas*,
                     blink::WebThemeEngine::Part,
                     blink::WebThemeEngine::State,
                     const blink::WebRect&,
                     const blink::WebThemeEngine::ExtraParams*);
#endif  // !defined(OS_MACOSX)
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
