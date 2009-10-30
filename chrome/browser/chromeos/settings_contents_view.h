// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_CONTENTS_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_CONTENTS_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"

namespace chromeos {

// Contents of the settings page for Chrome OS
class SettingsContentsView : public OptionsPageView {
 public:
  explicit SettingsContentsView(Profile* profile) : OptionsPageView(profile) {}
  virtual ~SettingsContentsView() {}

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

  DISALLOW_COPY_AND_ASSIGN(SettingsContentsView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_CONTENTS_VIEW_H_
