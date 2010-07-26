// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_SYSTEM_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_SYSTEM_PAGE_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/options/settings_page_view.h"

namespace chromeos {

// System settings page for Chrome OS
class SystemPageView : public SettingsPageView {
 public:
  explicit SystemPageView(Profile* profile) : SettingsPageView(profile) {}
  virtual ~SystemPageView() {}

 protected:
  // SettingsPageView implementation:
  virtual void InitControlLayout();

  DISALLOW_COPY_AND_ASSIGN(SystemPageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_SYSTEM_PAGE_VIEW_H_
