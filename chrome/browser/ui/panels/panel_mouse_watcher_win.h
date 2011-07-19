// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_WIN_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_WIN_H_
#pragma once

void EnsureMouseWatcherStarted();

void StopMouseWatcher();

bool IsMouseWatcherStarted();

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_WIN_H_
