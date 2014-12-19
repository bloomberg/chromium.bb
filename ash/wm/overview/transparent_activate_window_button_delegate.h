// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// A delegate that

#ifndef ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_DELEGATE_H_
#define ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// A pure-virtual delegate that is used by the TransparentActivateWindowButton
// to manipulate windows while in overview mode.
class ASH_EXPORT TransparentActivateWindowButtonDelegate {
 public:
  // Called when the TransparentActivateWindowButton was selected.
  virtual void Select() = 0;

 protected:
  TransparentActivateWindowButtonDelegate() {}
  virtual ~TransparentActivateWindowButtonDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TransparentActivateWindowButtonDelegate);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_DELEGATE_H_
