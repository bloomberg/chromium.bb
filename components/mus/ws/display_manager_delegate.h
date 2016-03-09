// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_

namespace mus {
namespace ws {

class Display;

class DisplayManagerDelegate {
 public:
  virtual void OnFirstDisplayReady() = 0;
  virtual void OnNoMoreDisplays() = 0;

 protected:
  virtual ~DisplayManagerDelegate() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
