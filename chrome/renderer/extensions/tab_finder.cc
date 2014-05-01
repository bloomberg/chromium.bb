// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/tab_finder.h"

#include "content/public/renderer/render_view.h"
#include "extensions/renderer/extension_helper.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using content::RenderView;

namespace extensions {

// static
content::RenderView* TabFinder::Find(int tab_id) {
  TabFinder finder(tab_id);
  RenderView::ForEach(&finder);
  return finder.view_;
}

TabFinder::TabFinder(int tab_id) : tab_id_(tab_id), view_(NULL) {}

TabFinder::~TabFinder() {}

// Note: Visit returns false to terminate the iteration.
bool TabFinder::Visit(RenderView* render_view) {
  // Only interested in the top frame.
  if (render_view->GetWebView()->mainFrame()->parent())
    return true;

  ExtensionHelper* helper = ExtensionHelper::Get(render_view);
  if (helper && helper->tab_id() == tab_id_)
    view_ = render_view;

  return !view_;
}

}  // namespace extensions
