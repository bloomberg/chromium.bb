// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace content {

class NotificationDatabase;
struct NotificationDatabaseData;

// Implementation of the Web Notification storage context.
//
// Represents the storage context for persistent Web Notifications, specific to
// the storage partition owning the instance. The public methods defined in this
// interface must only be called on the IO thread.
class CONTENT_EXPORT PlatformNotificationContext
    : public base::RefCountedThreadSafe<PlatformNotificationContext> {
 public:
  // Constructs a new platform notification context. If |path| is non-empty, the
  // database will be initialized in the "Platform Notifications" subdirectory
  // of |path|. Otherwise, the database will be initialized in memory.
  explicit PlatformNotificationContext(const base::FilePath& path);

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
  void ReadNotificationData(int64_t notification_id,
                            const GURL& origin,
                            const ReadResultCallback& callback);

  // Writes the data associated with a notification to a database. When this
  // action completed, |callback| will be invoked with the success status and
  // the persistent notification id when written successfully.
  void WriteNotificationData(const GURL& origin,
                             const NotificationDatabaseData& database_data,
                             const WriteResultCallback& callback);

  // Deletes all data associated with |notification_id| belonging to |origin|
  // from the database. |callback| will be invoked with the success status
  // when the operation has completed.
  void DeleteNotificationData(int64_t notification_id,
                              const GURL& origin,
                              const DeleteResultCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<PlatformNotificationContext>;
  friend class PlatformNotificationContextTest;

  virtual ~PlatformNotificationContext();

  // Initializes the database if neccesary. Must be called on the IO thread.
  // |success_closure| will be invoked on a the |task_runner_| thread when
  // everything is available, or |failure_closure_| will be invoked on the
  // IO thread when initialization fails.
  void LazyInitialize(const base::Closure& success_closure,
                      const base::Closure& failure_closure);

  // Opens the database. Must be called on the |task_runner_| thread. When the
  // database has been opened, |success_closure| will be invoked on the task
  // thread, otherwise |failure_closure_| will be invoked on the IO thread.
  void OpenDatabase(const base::Closure& success_closure,
                    const base::Closure& failure_closure);

  // Actually reads the notification data from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoReadNotificationData(int64_t notification_id,
                              const GURL& origin,
                              const ReadResultCallback& callback);

  // Actually writes the notification database to the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoWriteNotificationData(const GURL& origin,
                               const NotificationDatabaseData& database_data,
                               const WriteResultCallback& callback);

  // Actually deletes the notification information from the database. Must only
  // be called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoDeleteNotificationData(int64_t notification_id,
                                const GURL& origin,
                                const DeleteResultCallback& callback);

  // Returns the path in which the database should be initialized. May be empty.
  base::FilePath GetDatabasePath() const;

  // Sets the task runner to use for testing purposes.
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  base::FilePath path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<NotificationDatabase> database_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_H_
