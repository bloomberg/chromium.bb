// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LAYOUT_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_LAYOUT_UTILS_H_

namespace views {
class GridLayout;
class View;
}  // namespace views

namespace layout_utils {

// Creates a panel GridLayout for |host|. A panel layout has specific insets
// applied to make it appropriate for use in a dialog or bubble. This function
// sets the layout manager of |host| to the returned GridLayout.
views::GridLayout* CreatePanelLayout(views::View* host);

}  // namespace layout_utils

#endif  // CHROME_BROWSER_UI_VIEWS_LAYOUT_UTILS_H_
