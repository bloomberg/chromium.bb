// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_WINDOW_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_WINDOW_H_
#pragma once

class TabContents;

namespace content {

class DevToolsFrontendWindowDelegate;

// Installs delegate for DevTools front-end loaded into |client_tab_contents|.
void SetupDevToolsFrontendDelegate(
    TabContents* client_tab_contents,
    DevToolsFrontendWindowDelegate* delegate);

}

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_WINDOW_H_
