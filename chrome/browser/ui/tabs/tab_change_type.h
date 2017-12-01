// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_CHANGE_TYPE_H_
#define CHROME_BROWSER_UI_TABS_TAB_CHANGE_TYPE_H_

// Enumeration of possible changes to tab state used by the UI.
enum class TabChangeType {
  // Everything changed.
  kAll,

  // Only the loading state changed.
  kLoadingOnly,

  // Only the title changed and page isn't loading. This is used to set the
  // attention state for background pinned tabs. This has to be one
  // notification since the title changed and loading state updates are
  // normally broadcast asynchronously, making it hard to tell if the page was
  // definitely done loading when the title changed.
  kTitleNotLoading
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_CHANGE_TYPE_H_
