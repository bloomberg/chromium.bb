// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_

#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace content {

class MockWebThemeEngine : public blink::WebThemeEngine {
 public:
  virtual ~MockWebThemeEngine() {}

  // blink::WebThemeEngine:
  virtual blink::WebSize getSize(blink::WebThemeEngine::Part);
  virtual void paint(blink::WebCanvas*,
                     blink::WebThemeEngine::Part,
                     blink::WebThemeEngine::State,
                     const blink::WebRect&,
                     const blink::WebThemeEngine::ExtraParams*);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
