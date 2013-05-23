// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include "cc/resources/raster_worker_pool.h"

namespace cc {

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  NULL,
                  RasterWorkerPool::Create(1),
                  1,
                  false,
                  NULL,
                  false) {}
}
