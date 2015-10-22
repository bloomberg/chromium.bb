// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_IMPL_H_
#define COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_IMPL_H_

#include "base/basictypes.h"
#include "components/html_viewer/blink_settings.h"

namespace blink {
class WebView;
}

namespace html_viewer {
struct WebPreferences;

// BlinkSettingsImpl configure blink for a non-testing HTMLViewer.
class BlinkSettingsImpl : public BlinkSettings {
 public:
  BlinkSettingsImpl();

  // Additional more time-consuming configuration.
  void Init() override;

  // interim code: pushes layout test mode and other interim settings
  // into Blink.
  void ApplySettingsToWebView(blink::WebView* view) const override;

 protected:
  void ApplyFontRendereringSettings() const;
  void ApplySettings(blink::WebView* view, const WebPreferences& prefs) const;

 private:
  bool experimental_webgl_enabled_;

  DISALLOW_COPY_AND_ASSIGN(BlinkSettingsImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_BLINK_SETTINGS_IMPL_H_
