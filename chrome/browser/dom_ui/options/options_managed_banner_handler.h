// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_OPTIONS_MANAGED_BANNER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_OPTIONS_MANAGED_BANNER_HANDLER_H_
#pragma once

#include "chrome/browser/policy/managed_prefs_banner_base.h"
#include "chrome/browser/options_window.h"

class DOMUI;

// Managed options banner handler.
// Controls the display of a banner if an options panel contains options
// that are under administator control.
class OptionsManagedBannerHandler : public policy::ManagedPrefsBannerBase {
 public:
  OptionsManagedBannerHandler(DOMUI* dom_ui, const string16& page_name,
                              OptionsPage page);
  virtual ~OptionsManagedBannerHandler() { }

 protected:
  // ManagedPrefsBannerBase implementation.
  virtual void OnUpdateVisibility();

 private:
  // Set the managed options banner to be visible or invisible.
  void SetupBannerVisibilty();

  DOMUI* dom_ui_;  // weak reference to the dom-ui.
  string16 page_name_;  // current options page name.
  OptionsPage page_;  // current options page value.

  DISALLOW_COPY_AND_ASSIGN(OptionsManagedBannerHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_OPTIONS_MANAGED_BANNER_HANDLER_H_
