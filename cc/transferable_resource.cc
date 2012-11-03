// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/transferable_resource.h"

namespace cc {

TransferableResource::TransferableResource()
    : id(0),
      format(0) {
}

TransferableResource::~TransferableResource() {
}

TransferableResourceList::TransferableResourceList()
    : sync_point(0) {
}

TransferableResourceList::~TransferableResourceList() {
}

}  // namespace cc
