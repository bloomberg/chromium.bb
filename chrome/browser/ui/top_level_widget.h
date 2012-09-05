// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOP_LEVEL_WIDGET_H_
#define CHROME_BROWSER_UI_TOP_LEVEL_WIDGET_H_

class Browser;

namespace views {
class Widget;
}

namespace chrome {

views::Widget* GetTopLevelWidgetForBrowser(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TOP_LEVEL_WIDGET_H_
