// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_FACTORY_H_
#pragma once

class AbstractTabStripView;
class Browser;
class TabStripModel;

AbstractTabStripView* CreateTabStrip(Browser* browser,
                                     TabStripModel* model,
                                     bool use_vertical_tabs);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_FACTORY_H_
