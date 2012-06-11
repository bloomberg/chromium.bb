// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_DELEGATE_H_
#pragma once

class TabContents;

// Objects implement this interface to get notified about changes in the
// BlockedContentTabHelper and to provide necessary functionality.
class BlockedContentTabHelperDelegate {
 public:
  // If |source| is constrained, returns the tab containing it.  Otherwise
  // returns |source|.
  virtual TabContents* GetConstrainingTabContents(TabContents* source) = 0;

 protected:
  virtual ~BlockedContentTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_DELEGATE_H_
