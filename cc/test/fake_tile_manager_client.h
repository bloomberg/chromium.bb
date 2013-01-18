// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_TILE_MANAGER_CLIENT_H_
#define CC_TEST_FAKE_TILE_MANAGER_CLIENT_H_

#include "cc/tile_manager.h"

namespace cc {

class FakeTileManagerClient : public TileManagerClient {
 public:
  virtual ~FakeTileManagerClient() {}

  // TileManagerClient implementation.
  virtual void ScheduleManageTiles() OVERRIDE {}
  virtual void DidUploadVisibleHighResolutionTile() OVERRIDE {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_TILE_MANAGER_CLIENT_H_
