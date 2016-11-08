// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_ASH_VIEW_IDS_H_
#define ASH_COMMON_ASH_VIEW_IDS_H_

namespace ash {

enum ViewID {
  VIEW_ID_NONE = 0,

  // Ash IDs start above the range used in Chrome (c/b/ui/view_ids.h).
  VIEW_ID_MEDIA_TRAY_VIEW = 10000,
  VIEW_ID_USER_VIEW_MEDIA_INDICATOR,

  // View ID that is used to mark sticky header rows in a scroll view.
  VIEW_ID_STICKY_HEADER,
};

}  // namespace ash

#endif  // ASH_COMMON_ASH_VIEW_IDS_H_
