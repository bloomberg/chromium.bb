// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/user_id_tracker.h"

#include "components/mus/ws/user_id_tracker_observer.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace mus {
namespace ws {

UserIdTracker::UserIdTracker()
    : active_id_(mojo::shell::mojom::Connector::kInvalidApplicationID) {}
UserIdTracker::~UserIdTracker() {
  DCHECK(ids_.empty());
}

bool UserIdTracker::IsValidUserId(UserId id) const {
  return ids_.count(id) > 0;
}

void UserIdTracker::SetActiveUserId(UserId id) {
  if (id == active_id_)
    return;

  active_id_ = id;
  FOR_EACH_OBSERVER(UserIdTrackerObserver, observers_,
                    OnActiveUserIdChanged(id));
}

void UserIdTracker::AddUserId(UserId id) {
  DCHECK(!IsValidUserId(id));
  ids_.insert(id);
  FOR_EACH_OBSERVER(UserIdTrackerObserver, observers_, OnUserIdAdded(id));
}

void UserIdTracker::RemoveUserId(UserId id) {
  DCHECK(IsValidUserId(id));
  ids_.erase(id);
  FOR_EACH_OBSERVER(UserIdTrackerObserver, observers_, OnUserIdRemoved(id));
}

void UserIdTracker::AddObserver(UserIdTrackerObserver* observer) {
  observers_.AddObserver(observer);
}

void UserIdTracker::RemoveObserver(UserIdTrackerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ws
}  // namespace mus
