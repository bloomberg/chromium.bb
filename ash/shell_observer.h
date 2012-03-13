// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELLOBSERVER_H_
#define ASH_SHELLOBSERVER_H_
#pragma once

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT ShellObserver {
 public:
  // Invoked after the screen's work area insets changes.
  virtual void OnScreenWorkAreaInsetsChanged() {}

 protected:
  virtual ~ShellObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELLOBSERVER_H_
