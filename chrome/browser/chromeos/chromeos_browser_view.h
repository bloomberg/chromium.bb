// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEOS_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEOS_BROWSER_VIEW_H_

#include "chrome/browser/views/frame/browser_view.h"

class TabStripModel;

namespace chromeos {

// ChromeosBrowserView implements Chromeos specific behavior of
// BrowserView.
class ChromeosBrowserView : public BrowserView {
 public:
  explicit ChromeosBrowserView(Browser* browser);
  virtual ~ChromeosBrowserView() {}

  // BrowserView overrides.
  virtual void Show();
  virtual bool IsToolbarVisible() const;
  virtual void SetFocusToLocationBar();
  virtual views::LayoutManager* CreateLayoutManager() const;
  virtual TabStrip* CreateTabStrip(TabStripModel* tab_strip_model) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeosBrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEOS_BROWSER_VIEW_H_
