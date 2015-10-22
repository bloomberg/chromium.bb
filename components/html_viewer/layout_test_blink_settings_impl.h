// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_LAYOUT_TEST_BLINK_SETTINGS_IMPL_H_
#define COMPONENTS_HTML_VIEWER_LAYOUT_TEST_BLINK_SETTINGS_IMPL_H_

#include "base/basictypes.h"
#include "components/html_viewer/blink_settings_impl.h"

namespace html_viewer {
struct WebPreferences;

// LayoutTestBlinkSettingsImpl is a subclass of BlinkSettings that can
// configure Blink for running layout tests.
class LayoutTestBlinkSettingsImpl : public BlinkSettingsImpl {
 public:
  LayoutTestBlinkSettingsImpl();
  void Init() override;

 private:
  void ApplySettingsToWebView(blink::WebView* view) const override;

  bool accelerated_2d_canvas_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestBlinkSettingsImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_LAYOUT_TEST_BLINK_SETTINGS_IMPL_H_
