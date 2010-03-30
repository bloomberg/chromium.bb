// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_PLUGIN_FILTER_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_PLUGIN_FILTER_PAGE_VIEW_H_

#include "chrome/browser/views/options/content_filter_page_view.h"

////////////////////////////////////////////////////////////////////////////////
// PluginFilterPageView class is used to render the plugin content settings tab.

class PluginFilterPageView : public ContentFilterPageView,
                             public views::LinkController {
 public:
  explicit PluginFilterPageView(Profile* profile);
  virtual ~PluginFilterPageView();

 private:
  // Overridden from ContentFilterPageView:
  virtual void InitControlLayout();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PluginFilterPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_PLUGIN_FILTER_PAGE_VIEW_H_

