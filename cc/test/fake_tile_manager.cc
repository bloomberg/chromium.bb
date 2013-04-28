// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

namespace cc {

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client, NULL, 1, false, false, NULL) {
}
}
