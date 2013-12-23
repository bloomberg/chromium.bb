// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_GEOLOCATION_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_GEOLOCATION_OPTIONS_HANDLER_H_

#include "base/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// Handles processing of the geolocation options on settings page load.
class GeolocationOptionsHandler : public OptionsPageUIHandler {
 public:
  GeolocationOptionsHandler();
  virtual ~GeolocationOptionsHandler();

  // OptionsPageUIHandler implementation
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializePage() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_GEOLOCATION_OPTIONS_HANDLER_H_

