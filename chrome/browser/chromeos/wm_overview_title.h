// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_TITLE_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_TITLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"

class GURL;

namespace gfx {
class Size;
}

namespace views {
class Widget;
}

namespace chromeos {

class DropShadowLabel;
class WmOverviewSnapshot;

// WmOverviewTitle contains the title and URL of an associated tab
// snapshot.
class WmOverviewTitle {
 public:
  WmOverviewTitle();
  ~WmOverviewTitle();

  void Init(const gfx::Size& size, WmOverviewSnapshot* snapshot);

  void SetTitle(const string16& title);
  void SetUrl(const GURL& url);

  views::Widget* widget() { return widget_; }

 private:
  // This contains the title of the tab contents.
  DropShadowLabel* title_label_;

  // This contains the url of the tab contents.
  DropShadowLabel* url_label_;

  // Not owned, deletes itself when the underlying widget is destroyed.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewTitle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_TITLE_H_
