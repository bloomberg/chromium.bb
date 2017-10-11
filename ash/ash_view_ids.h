// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_VIEW_IDS_H_
#define ASH_ASH_VIEW_IDS_H_

namespace ash {

// TODO(jamescook): Move to //ash/public/cpp.
enum ViewID {
  VIEW_ID_NONE = 0,

  // Ash IDs start above the range used in Chrome (c/b/ui/view_ids.h).
  VIEW_ID_MEDIA_TRAY_VIEW = 10000,
  VIEW_ID_USER_VIEW_MEDIA_INDICATOR,
  VIEW_ID_BLUETOOTH_DEFAULT_VIEW,
  VIEW_ID_ACCESSIBILITY_TRAY_ITEM,
  VIEW_ID_TRAY_ENTERPRISE,
  VIEW_ID_EXTENSION_CONTROLLED_WIFI,

  // View ID that is used to mark sticky header rows in a scroll view.
  VIEW_ID_STICKY_HEADER,
};

}  // namespace ash

#endif  // ASH_ASH_VIEW_IDS_H_
