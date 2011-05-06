// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_TAB_HELPER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class TabContentsWrapper;

// Objects implement this interface to get notified about changes in the
// BookmarksTabHelper and to provide necessary functionality.
class BookmarksTabHelperDelegate {
 public:
  // Notification that the starredness of the current URL changed.
  virtual void URLStarredChanged(TabContentsWrapper* source, bool starred) = 0;

 protected:
  virtual ~BookmarksTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_TAB_HELPER_DELEGATE_H_
