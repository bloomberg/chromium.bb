// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_H_

#include <string>
#include <vector>

namespace libgtkui {

// This class is just a switch between NavButtonLayoutManagerGconf and
// NavButtonLayoutManagerGtk3.  Currently, it is empty and it's only
// purpose is so that GtkUi can store just a
// std::unique_ptr<NavButtonLayoutManager> and not have to have the
// two impls each guarded by their own macros.
class NavButtonLayoutManager {
 public:
  virtual ~NavButtonLayoutManager() {}

 protected:
  // Even though this class is not pure virtual, it should not be
  // instantiated directly.  Use NavButtonLayoutManagerGconf or
  // NavButtonLayoutManagerGtk3 instead.
  NavButtonLayoutManager() {}
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_H_
