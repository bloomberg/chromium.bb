// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_H_

#include "base/basictypes.h"

namespace blink {
class WebView;
}

namespace html_viewer {

// BlinkSettings encapsulates the necessary settings needed by the blink
// instance used by this HTMLViewer.
//
// TODO(rjkroege): The methods labeled "interim" are waiting on the a
// future rationaliztion of the code comprising blink::WebSettings,
// content::WebPreferences etc. per http://crbug.com/481108
//
// TODO(rjkroege): Support configuring preferences via mojo as necessary.
class BlinkSettings {
 public:
  BlinkSettings();
  ~BlinkSettings();

  // Additional more time-consuming configuration.
  void Init();

  // Marks this |BlinkSettings| object to setup blink for running layout tests.
  void SetLayoutTestMode();

  // interim code: pushes layout test mode and other interim settings
  // into Blink.
  void ApplySettingsToWebView(blink::WebView* view) const;

 private:
  bool layout_test_settings_;
  bool experimental_webgl_enabled_;

  DISALLOW_COPY_AND_ASSIGN(BlinkSettings);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_H_
