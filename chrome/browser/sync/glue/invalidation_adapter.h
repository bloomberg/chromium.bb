// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_INVALIDATION_ADAPTER_H_
#define CHROME_BROWSER_SYNC_GLUE_INVALIDATION_ADAPTER_H_

#include "components/invalidation/invalidation.h"
#include "sync/internal_api/public/base/invalidation_interface.h"

namespace browser_sync {

// Wraps a syncer::Invalidation in the syncer::InvalidationInterface.
class InvalidationAdapter : public syncer::InvalidationInterface {
 public:
  explicit InvalidationAdapter(const syncer::Invalidation& invalidation);
  ~InvalidationAdapter() override;

  // Implementation of InvalidationInterface.
  bool IsUnknownVersion() const override;
  const std::string& GetPayload() const override;
  int64 GetVersion() const override;
  void Acknowledge() override;
  void Drop() override;

 private:
  syncer::Invalidation invalidation_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_INVALIDATION_ADAPTER_H_
