// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_GESTURE_CONFIG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_GESTURE_CONFIG_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace base {
  class ListValue;
}  // namespace base

// The WebUI for 'chrome://gesture'.
class GestureConfigUI : public content::WebUIController {
 public:
  // Constructs a new GestureConfig for the specified |web_ui|.
  explicit GestureConfigUI(content::WebUI* web_ui);
  virtual ~GestureConfigUI();

 private:
  // TODO(mohsen): Add a whitelist of preferences that are allowed to be set or
  // get here and check requested preferences against this whitelist.

  /**
   * Request a preference setting's value.
   * This method is asynchronous; the result is provided by a call to
   * the JS method 'gesture_config.updatePreferenceValueResult'.
   */
  void UpdatePreferenceValue(const base::ListValue* args);

  /**
   * Reset a preference to its default value and return this value
   * via asynchronous callback to the JS method
   * 'gesture_config.updatePreferenceValueResult'.
   */
  void ResetPreferenceValue(const base::ListValue* args);

  /**
   * Set a preference setting's value.
   * Two parameters are provided in a JS list: prefName and value, the
   * key of the preference value to be set, and the value it's to be set to.
   */
  void SetPreferenceValue(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(GestureConfigUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_GESTURE_CONFIG_UI_H_
