// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_AREA_VIEW_H_

#include "base/basictypes.h"
#include "views/view.h"

namespace chromeos {

class ClockMenuButton;
class LanguageMenuButton;
class NetworkMenuButton;
class PowerMenuButton;
class StatusAreaHost;

// This class is used to wrap the small informative widgets in the upper-right
// of the window title bar. It is used on ChromeOS only.
class StatusAreaView : public views::View {
 public:
  enum OpenTabsMode {
    OPEN_TABS_ON_LEFT = 1,
    OPEN_TABS_CLOBBER,
    OPEN_TABS_ON_RIGHT
  };

  explicit StatusAreaView(StatusAreaHost* host);
  virtual ~StatusAreaView() {}

  virtual void Init();

  // Called when the compact navigation bar mode has changed to
  // toggle the app menu visibility.
  void Update();

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  static OpenTabsMode GetOpenTabsMode();
  static void SetOpenTabsMode(OpenTabsMode mode);

 private:
  StatusAreaHost* host_;

  ClockMenuButton* clock_view_;
  LanguageMenuButton* language_view_;
  NetworkMenuButton* network_view_;
  PowerMenuButton* power_view_;

  static OpenTabsMode open_tabs_mode_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_AREA_VIEW_H_
