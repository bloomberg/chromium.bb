// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_TITLE_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_TITLE_H_
#pragma once

#include "base/string16.h"
#include "views/widget/widget_gtk.h"

class Browser;
class GURL;

namespace gfx {
class Size;
}

namespace chromeos {

class DropShadowLabel;
class WmOverviewSnapshot;

// WmOverviewTitle contains the title and URL of an associated tab
// snapshot.
class WmOverviewTitle : public views::WidgetGtk {
 public:
  WmOverviewTitle();
  void Init(const gfx::Size& size, WmOverviewSnapshot* snapshot);

  void SetTitle(const string16& title);
  void SetUrl(const GURL& url);

 private:
  // This contains the title of the tab contents.
  DropShadowLabel* title_label_;

  // This contains the url of the tab contents.
  DropShadowLabel* url_label_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewTitle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_TITLE_H_
