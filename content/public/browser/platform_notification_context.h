// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_CONTEXT_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace content {

struct NotificationDatabaseData;

// Represents the storage context for persistent Web Notifications, specific to
// the storage partition owning the instance. All methods defined in this
// interface may only be used on the IO thread.
class PlatformNotificationContext
    : public base::RefCountedThreadSafe<PlatformNotificationContext> {
 public:
  using ReadResultCallback =
      base::Callback<void(bool /* success */,
                          const NotificationDatabaseData&)>;

  using WriteResultCallback =
      base::Callback<void(bool /* success */,
                          int64_t /* notification_id */)>;

  using DeleteResultCallback = base::Callback<void(bool /* success */)>;

  // Reads the data associated with |notification_id| belonging to |origin|
  // from the database. |callback| will be invoked with the success status
  // and a reference to the notification database data when completed.
  virtual void ReadNotificationData(int64_t notification_id,
                                    const GURL& origin,
                                    const ReadResultCallback& callback) = 0;

  // Writes the data associated with a notification to a database. When this
  // action completed, |callback| will be invoked with the success status and
  // the persistent notification id when written successfully.
  virtual void WriteNotificationData(
      const GURL& origin,
      const NotificationDatabaseData& database_data,
      const WriteResultCallback& callback) = 0;

  // Deletes all data associated with |notification_id| belonging to |origin|
  // from the database. |callback| will be invoked with the success status
  // when the operation has completed.
  virtual void DeleteNotificationData(int64_t notification_id,
                                      const GURL& origin,
                                      const DeleteResultCallback& callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<PlatformNotificationContext>;

  virtual ~PlatformNotificationContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_CONTEXT_H_
