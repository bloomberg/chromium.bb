// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ATHENA_ATHENA_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_ATHENA_ATHENA_UTIL_H_

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

// Returns the web content associated with the window.
content::WebContents* GetWebContentsForWindow(aura::Window* owner_window);

#endif  // CHROME_BROWSER_UI_VIEWS_ATHENA_ATHENA_UTIL_H_
