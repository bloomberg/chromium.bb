// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SALSA_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SALSA_UI_H_

#include <map>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
class Value;
}  // namespace base

// The WebUI for 'chrome://salsa' -- a user front end for the touch UI
// blind user studies.  I does this by setting gesture preference values
// without the user knowing which ones and having them try out various
// settings to see which results in a better experience for them.
class SalsaUI : public content::WebUIController {
 public:
  // Constructs a new GestureConfig for the specified |web_ui|.
  explicit SalsaUI(content::WebUI* web_ui);
  virtual ~SalsaUI();

 private:
  // Set a preference setting's value.
  // Two parameters are provided in a JS list: prefName and value, the
  // key of the preference value to be set, and the value it's to be set to.
  void SetPreferenceValue(const base::ListValue* args);

  // Record the current value for a preference key and store the key/value pair
  // in the member variable orig_values_.
  void BackupPreferenceValue(const base::ListValue* args);

  // Check and see if a key is on the whitelist.  Returns the index into
  // the whitelist on success and -1 on failure.
  int WhitelistIndex(const char* key) const;

  std::map<int, const base::Value*> orig_values_;

  DISALLOW_COPY_AND_ASSIGN(SalsaUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SALSA_UI_H_

