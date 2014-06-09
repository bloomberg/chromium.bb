// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_ACTIVITY_H_
#define COMPONENTS_GCM_DRIVER_GCM_ACTIVITY_H_

#include <string>
#include <vector>

#include "base/time/time.h"

namespace gcm {

// Contains data that are common to all activity kinds below.
struct Activity {
  Activity();
  virtual ~Activity();

  base::Time time;
  std::string event;    // A short description of the event.
  std::string details;  // Any additional detail about the event.
};

// Contains relevant data of a connection activity.
struct ConnectionActivity : Activity {
  ConnectionActivity();
  virtual ~ConnectionActivity();
};

// Contains relevant data of a check-in activity.
struct CheckinActivity : Activity {
  CheckinActivity();
  virtual ~CheckinActivity();
};

// Contains relevant data of a registration/unregistration step.
struct RegistrationActivity : Activity {
  RegistrationActivity();
  virtual ~RegistrationActivity();

  std::string app_id;
  std::string sender_ids;  // Comma separated sender ids.
};

// Contains relevant data of a message receiving event.
struct ReceivingActivity : Activity {
  ReceivingActivity();
  virtual ~ReceivingActivity();

  std::string app_id;
  std::string from;
  int message_byte_size;
};

// Contains relevant data of a send-message step.
struct SendingActivity : Activity {
  SendingActivity();
  virtual ~SendingActivity();

  std::string app_id;
  std::string receiver_id;
  std::string message_id;
};

struct RecordedActivities {
  RecordedActivities();
  virtual ~RecordedActivities();

  std::vector<CheckinActivity> checkin_activities;
  std::vector<ConnectionActivity> connection_activities;
  std::vector<RegistrationActivity> registration_activities;
  std::vector<ReceivingActivity> receiving_activities;
  std::vector<SendingActivity> sending_activities;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_ACTIVITY_H_
