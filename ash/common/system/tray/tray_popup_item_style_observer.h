// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_ITEM_STYLE_OBSERVER_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_ITEM_STYLE_OBSERVER_H_

namespace ash {

class TrayPopupItemStyleObserver {
 public:
  // Notified by TrayPopupItemStyle instances when the Style is updated (even if
  // the style values aren't changing).
  virtual void OnTrayPopupItemStyleUpdated() = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_ITEM_STYLE_OBSERVER_H_
