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

// BlinkSettings is a pure-virtual interface for configuring blink for
// use by this HTMLViewer.
//
// TODO(rjkroege): The methods labeled "interim" are waiting on the a
// future rationaliztion of the code comprising blink::WebSettings,
// content::WebPreferences etc. per http://crbug.com/481108
//
// TODO(rjkroege): Support configuring preferences via mojo as necessary.
class BlinkSettings {
 public:
  BlinkSettings();
  virtual ~BlinkSettings();

  // Additional more time-consuming configuration.
  virtual void Init() = 0;

  // interim code: pushes settings into Blink.
  virtual void ApplySettingsToWebView(blink::WebView* view) const = 0;

  DISALLOW_COPY_AND_ASSIGN(BlinkSettings);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_H_
