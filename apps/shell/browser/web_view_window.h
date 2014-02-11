// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_WEB_VIEW_WINDOW_H_
#define APPS_SHELL_BROWSER_WEB_VIEW_WINDOW_H_

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

namespace apps {

// Create an example window containing a WebView.
views::Widget* CreateWebViewWindow(content::BrowserContext* browser_context,
                                   aura::Window* window_context);

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_WEB_VIEW_WINDOW_H_
