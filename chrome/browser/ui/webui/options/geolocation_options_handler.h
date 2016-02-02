// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_GEOLOCATION_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_GEOLOCATION_OPTIONS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/prefs/pref_member.h"

namespace options {

// Handles processing of the geolocation options on settings page load.
class GeolocationOptionsHandler : public OptionsPageUIHandler {
 public:
  GeolocationOptionsHandler();
  ~GeolocationOptionsHandler() override;

  // OptionsPageUIHandler implementation
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_GEOLOCATION_OPTIONS_HANDLER_H_

