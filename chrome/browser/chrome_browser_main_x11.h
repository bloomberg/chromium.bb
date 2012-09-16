// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are gtk-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_X11_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_X11_H_

// Installs the X11 error handlers for the browser process. This will
// allow us to exit cleanly if X exits before us.
void SetBrowserX11ErrorHandlers();

// Installs empty X11 error handlers. This avoids calling into the message-loop
// in case an X11 erro happens while the message-loop is being destroyed.
void UnsetBrowserX11ErrorHandlers();

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_X11_H_
