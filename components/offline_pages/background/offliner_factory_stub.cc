// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/offliner_factory_stub.h"

#include "components/offline_pages/background/offliner_stub.h"

namespace offline_pages {

OfflinerFactoryStub::OfflinerFactoryStub() : offliner_(nullptr) {}

OfflinerFactoryStub::~OfflinerFactoryStub() {}

Offliner* OfflinerFactoryStub::GetOffliner(const OfflinerPolicy* policy) {
  if (offliner_.get() == nullptr) {
    offliner_.reset(new OfflinerStub());
  }
  return offliner_.get();
}

}  // namespace offline_pages
