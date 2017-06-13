// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_UPDATE_OBSERVER_H_
#define ASH_SYSTEM_UPDATE_UPDATE_OBSERVER_H_

namespace ash {

class UpdateObserver {
 public:
  virtual ~UpdateObserver() {}

  virtual void OnUpdateOverCellularTargetSet(bool success) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_UPDATE_OBSERVER_H_
