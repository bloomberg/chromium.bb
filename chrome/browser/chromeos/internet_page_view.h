// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INTERNET_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INTERNET_PAGE_VIEW_H_

#include "chrome/browser/chromeos/settings_page_view.h"

namespace chromeos {

// Internet settings page for Chrome OS
class InternetPageView : public SettingsPageView {
 public:
  explicit InternetPageView(Profile* profile) : SettingsPageView(profile) {}
  virtual ~InternetPageView() {}

 protected:
  // SettingsPageView implementation:
  virtual void InitControlLayout();

  DISALLOW_COPY_AND_ASSIGN(InternetPageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INTERNET_PAGE_VIEW_H_
