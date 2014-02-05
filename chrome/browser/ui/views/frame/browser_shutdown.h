// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_SHUTDOWN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_SHUTDOWN_H_

class Browser;

// Destroys all the WebContents in |browser|s TabStrip.
void DestroyBrowserWebContents(Browser* browser);

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_SHUTDOWN_H_
