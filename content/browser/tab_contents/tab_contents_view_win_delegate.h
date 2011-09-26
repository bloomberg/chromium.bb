// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_WIN_DELEGATE_H_
#define CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_WIN_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class TabContents;
class TabContentsViewWin;
struct ViewHostMsg_CreateWindow_Params;

// A delegate interface for TabContentsViewWin.
class TabContentsViewWinDelegate {
 public:
  virtual ~TabContentsViewWinDelegate() {}

  // These match the TabContentsView methods.
  virtual TabContents* CreateNewWindow(
      TabContentsViewWin* tab_contents_view,
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) = 0;

 protected:
  TabContentsViewWinDelegate() {}

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewWinDelegate);
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_WIN_DELEGATE_H_
