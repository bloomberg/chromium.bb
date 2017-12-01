// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"

namespace syncer {

class ModelTypeSyncBridge;

// This implementation is intended to be used in unit tests, with public
// accessors that allow reading all data to verify expectations.
class FakeUserEventService : public UserEventService {
 public:
  FakeUserEventService();
  ~FakeUserEventService() override;

  // UserEventService implementation.
  void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) override;
  void RecordUserEvent(const sync_pb::UserEventSpecifics& specifics) override;
  base::WeakPtr<ModelTypeSyncBridge> GetSyncBridge() override;

  const std::vector<sync_pb::UserEventSpecifics>& GetRecordedUserEvents() const;

 private:
  std::vector<sync_pb::UserEventSpecifics> recorded_user_events_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserEventService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
