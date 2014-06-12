// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_
#define COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace invalidation {

class InvalidationService;

// A KeyedService that owns an InvalidationService.
class ProfileInvalidationProvider : public KeyedService {
 public:
  explicit ProfileInvalidationProvider(
      scoped_ptr<InvalidationService> invalidation_service);
  virtual ~ProfileInvalidationProvider();

  InvalidationService* GetInvalidationService();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

 private:
  scoped_ptr<InvalidationService> invalidation_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInvalidationProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_
