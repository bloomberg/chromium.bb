// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_TYPES_H_
#define CHROME_BROWSER_UI_SAD_TAB_TYPES_H_

namespace chrome {

// NOTE: Do not remove or reorder the elements in this enum, and only add new
// items at the end. We depend on these specific values in a histogram.
enum SadTabKind {
  SAD_TAB_KIND_CRASHED = 0,  // Tab crashed. Display the "Aw, Snap!" page.
  SAD_TAB_KIND_KILLED  // Tab killed. Display the "He's dead, Jim!" tab page.
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SAD_TAB_TYPES_H_
