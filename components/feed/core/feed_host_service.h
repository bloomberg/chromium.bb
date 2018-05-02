// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_HOST_SERVICE_H_
#define COMPONENTS_FEED_CORE_FEED_HOST_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace feed {

// KeyedService responsible for managing the lifetime of Feed Host API
// implementations. It instantiates and owns these API implementations, and
// provides access to non-owning pointers to them. While host implementations
// may be created on demand, it is possible they will not be fully initialized
// yet.
class FeedHostService : public KeyedService {
 public:
  FeedHostService();
  ~FeedHostService() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedHostService);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_HOST_SERVICE_H_
