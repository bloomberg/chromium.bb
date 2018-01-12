// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_id_generator.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kNotificationTagSeparator[] = "#";
const char kPersistentNotificationPrefix[] = "p";
const char kNonPersistentNotificationPrefix[] = "n";

}  // namespace

// static
bool NotificationIdGenerator::IsPersistentNotification(
    const base::StringPiece& notification_id) {
  return notification_id.starts_with(
      std::string(kPersistentNotificationPrefix));
}

// static
bool NotificationIdGenerator::IsNonPersistentNotification(
    const base::StringPiece& notification_id) {
  return notification_id.starts_with(
      std::string(kNonPersistentNotificationPrefix));
}

// Notification Id is of the following format:
// p#<origin>#[1|0][<developer_tag>|persistent_notification_id]
std::string NotificationIdGenerator::GenerateForPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    int64_t persistent_notification_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.GetOrigin());

  std::stringstream stream;

  stream << kPersistentNotificationPrefix << kNotificationTagSeparator;
  stream << origin;
  stream << kNotificationTagSeparator;

  stream << base::IntToString(!tag.empty());
  if (tag.size())
    stream << tag;
  else
    stream << base::Int64ToString(persistent_notification_id);

  return stream.str();
}

// Notification Id is of the following format:
// n#<origin>#[1<developer_tag>|0<render_process_id>#<request_id>]
std::string NotificationIdGenerator::GenerateForNonPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    int request_id,
    int render_process_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.GetOrigin());

  std::stringstream stream;

  stream << kNonPersistentNotificationPrefix << kNotificationTagSeparator;
  stream << origin;
  stream << kNotificationTagSeparator;

  stream << base::IntToString(!tag.empty());
  if (tag.empty()) {
    stream << base::IntToString(render_process_id);
    stream << kNotificationTagSeparator;

    stream << base::IntToString(request_id);
  } else {
    stream << tag;
  }

  return stream.str();
}

}  // namespace content
