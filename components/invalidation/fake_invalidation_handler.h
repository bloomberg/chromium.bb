// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_FAKE_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_FAKE_INVALIDATION_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/invalidation/invalidation_handler.h"
#include "components/invalidation/object_id_invalidation_map.h"

namespace syncer {

class FakeInvalidationHandler : public InvalidationHandler {
 public:
  FakeInvalidationHandler();
  virtual ~FakeInvalidationHandler();

  InvalidatorState GetInvalidatorState() const;
  const ObjectIdInvalidationMap& GetLastInvalidationMap() const;
  int GetInvalidationCount() const;

  // InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual std::string GetOwnerName() const OVERRIDE;

 private:
  InvalidatorState state_;
  ObjectIdInvalidationMap last_invalidation_map_;
  int invalidation_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationHandler);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_FAKE_INVALIDATION_HANDLER_H_
