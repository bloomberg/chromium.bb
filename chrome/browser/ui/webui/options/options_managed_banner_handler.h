// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_MANAGED_BANNER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_MANAGED_BANNER_HANDLER_H_
#pragma once

#include "base/string16.h"
#include "chrome/browser/policy/managed_prefs_banner_base.h"
#include "chrome/browser/ui/options/options_window.h"

class WebUI;

// Managed options banner handler.
// Controls the display of a banner if an options panel contains options
// that are under administator control.
class OptionsManagedBannerHandler : public policy::ManagedPrefsBannerBase {
 public:
  OptionsManagedBannerHandler(WebUI* web_ui, const string16& page_name,
                              OptionsPage page);
  virtual ~OptionsManagedBannerHandler();

 protected:
  // ManagedPrefsBannerBase implementation.
  virtual void OnUpdateVisibility();

 private:
  // Set the managed options banner to be visible or invisible.
  void SetupBannerVisibility();

  WebUI* web_ui_;  // weak reference to the WebUI.
  string16 page_name_;  // current options page name.
  OptionsPage page_;  // current options page value.

  DISALLOW_COPY_AND_ASSIGN(OptionsManagedBannerHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_MANAGED_BANNER_HANDLER_H_
