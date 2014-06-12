// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager_client.h"

#include <vector>

namespace cc {

FakeTileManagerClient::FakeTileManagerClient() {
}

FakeTileManagerClient::~FakeTileManagerClient() {
}

const std::vector<PictureLayerImpl*>&
FakeTileManagerClient::GetPictureLayers() {
  return picture_layers_;
}

}  // namespace cc
