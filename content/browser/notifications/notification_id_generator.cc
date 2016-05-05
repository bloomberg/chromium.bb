// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_id_generator.h"

#include <sstream>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kPersistentPrefix[] = "p:";
const char kNonPersistentPrefix[] = "n:";

const char kSeparator = '#';

// Computes a hash based on the path in which the |browser_context| is stored.
// Since we only store the hash, SHA-1 is used to make the probability of
// collisions negligible.
std::string ComputeBrowserContextHash(BrowserContext* browser_context) {
  const base::FilePath path = browser_context->GetPath();

#if defined(OS_WIN)
  return base::SHA1HashString(base::WideToUTF8(path.value()));
#else
  return base::SHA1HashString(path.value());
#endif
}

}  // namespace

NotificationIdGenerator::NotificationIdGenerator(
    BrowserContext* browser_context,
    int render_process_id)
    : browser_context_(browser_context),
      render_process_id_(render_process_id) {}

NotificationIdGenerator::~NotificationIdGenerator() {}

// static
bool NotificationIdGenerator::IsPersistentNotification(
    const base::StringPiece& notification_id) {
  return notification_id.starts_with(kPersistentPrefix);
}

// static
bool NotificationIdGenerator::IsNonPersistentNotification(
    const base::StringPiece& notification_id) {
  return notification_id.starts_with(kNonPersistentPrefix);
}

std::string NotificationIdGenerator::GenerateForPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    int64_t persistent_notification_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.GetOrigin());

  std::stringstream stream;

  stream << kPersistentPrefix;
  stream << ComputeBrowserContextHash(browser_context_);
  stream << browser_context_->IsOffTheRecord();
  stream << origin;

  // Persistent notification ids are unique for the lifetime of the notification
  // database, orthogonal to the renderer that created the notification.

  stream << !!tag.size();
  if (tag.size())
    stream << tag;
  else
    stream << persistent_notification_id;

  return stream.str();
}

std::string NotificationIdGenerator::GenerateForNonPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    int non_persistent_notification_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.GetOrigin());

  std::stringstream stream;

  stream << kNonPersistentPrefix;
  stream << ComputeBrowserContextHash(browser_context_);
  stream << browser_context_->IsOffTheRecord();
  stream << origin;

  // Non-persistent notification ids are unique per renderer process when no
  // tag is being used. Tags still identify uniqueness for the given origin.

  stream << !!tag.size();
  if (!tag.size()) {
    stream << render_process_id_;
    stream << kSeparator;

    stream << non_persistent_notification_id;
  } else {
    stream << tag;
  }

  return stream.str();
}

}  // namespace content
