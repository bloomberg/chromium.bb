// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/profile_invalidation_provider.h"

#include "components/invalidation/invalidation_service.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider(
    scoped_ptr<InvalidationService> invalidation_service)
    : invalidation_service_(invalidation_service.Pass()) {
}

ProfileInvalidationProvider::~ProfileInvalidationProvider() {
}

InvalidationService* ProfileInvalidationProvider::GetInvalidationService() {
  return invalidation_service_.get();
}

void ProfileInvalidationProvider::Shutdown() {
  invalidation_service_.reset();
}

}  // namespace invalidation
