// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_

// This file contains functions for running a variety of browser dialogs and
// popups. The dialogs here are the ones that the caller does not need to
// access the class of the popup. It allows us to break dependencies by
// allowing the callers to not depend on the classes implementing the dialogs.
// TODO: Make as many of these methods as possible cross platform, and move them
// into chrome/browser/ui/browser_dialogs.h.

class BrowserView;
class FindBar;

namespace chrome {

// Creates and returns a find bar for the given browser window. See FindBarWin.
FindBar* CreateFindBar(BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
