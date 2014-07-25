// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_application_id.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace {
const char kSeparator = '#';  // Ok as only the origin of the url is used.
}

namespace gcm {

const char kPushMessagingApplicationIdPrefix[] = "push";

// static
PushMessagingApplicationId PushMessagingApplicationId::Parse(
    const std::string& id) {
  std::vector<std::string> parts;
  base::SplitString(id, kSeparator, &parts);

  if (parts.size() != 3 || parts[0] != kPushMessagingApplicationIdPrefix)
    return PushMessagingApplicationId();

  GURL origin = GURL(parts[1]);
  if (!origin.is_valid() || origin.GetOrigin() != origin)
    return PushMessagingApplicationId();

  int64 service_worker_registration_id;
  if (!base::StringToInt64(parts[2], &service_worker_registration_id))
    return PushMessagingApplicationId();

  return PushMessagingApplicationId(origin, service_worker_registration_id);
}

bool PushMessagingApplicationId::IsValid() {
  return origin.is_valid() && origin.GetOrigin() == origin &&
         service_worker_registration_id >= 0;
}

std::string PushMessagingApplicationId::ToString() const {
  return (std::string(kPushMessagingApplicationIdPrefix) + kSeparator +
          origin.spec() + kSeparator +
          base::Int64ToString(service_worker_registration_id));
}

}  // namespace gcm
