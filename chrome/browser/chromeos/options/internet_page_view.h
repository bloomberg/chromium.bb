// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_INTERNET_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_INTERNET_PAGE_VIEW_H_

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/options/settings_page_view.h"

namespace views {
class ScrollView;
}  // namespace views

namespace chromeos {

class InternetPageContentView;

// Internet settings page for Chrome OS
class InternetPageView : public SettingsPageView,
                         public NetworkLibrary::Observer {
 public:
  explicit InternetPageView(Profile* profile);
  virtual ~InternetPageView();

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(NetworkLibrary* obj);
  virtual void NetworkTraffic(NetworkLibrary* obj, int traffic_type) {}

  // views::View overrides:
  virtual void Layout();

 protected:
  // SettingsPageView implementation:
  virtual void InitControlLayout();

 private:
  // The contents of the internet page view.
  InternetPageContentView* contents_view_;

  // The scroll view that contains the advanced options.
  views::ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(InternetPageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_INTERNET_PAGE_VIEW_H_
