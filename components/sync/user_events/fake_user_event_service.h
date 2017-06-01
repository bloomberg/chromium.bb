// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_

#include <map>
#include <memory>
#include <set>
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
  void RegisterDependentFieldTrial(
      const std::string& trial_name,
      sync_pb::UserEventSpecifics::EventCase event_case) override;
  base::WeakPtr<ModelTypeSyncBridge> GetSyncBridge() override;

  const std::vector<sync_pb::UserEventSpecifics>& GetRecordedUserEvents() const;
  const std::map<std::string, std::set<sync_pb::UserEventSpecifics::EventCase>>&
  GetRegisteredDependentFieldTrials() const;

 private:
  std::vector<sync_pb::UserEventSpecifics> recorded_user_events_;
  std::map<std::string, std::set<sync_pb::UserEventSpecifics::EventCase>>
      registered_dependent_field_trials_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserEventService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
