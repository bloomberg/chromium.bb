// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context_impl.h"

#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/notifications/notification_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"

namespace content {

// Name of the directory in the user's profile directory where the notification
// database files should be stored.
const base::FilePath::CharType kPlatformNotificationsDirectory[] =
    FILE_PATH_LITERAL("Platform Notifications");

PlatformNotificationContextImpl::PlatformNotificationContextImpl(
    const base::FilePath& path)
    : path_(path) {
}

PlatformNotificationContextImpl::~PlatformNotificationContextImpl() {
  // If the database has been initialized, it must be deleted on the task runner
  // thread as closing it may cause file I/O.
  if (database_) {
    DCHECK(task_runner_);
    task_runner_->DeleteSoon(FROM_HERE, database_.release());
  }
}

void PlatformNotificationContextImpl::ReadNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const ReadResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
       base::Bind(&PlatformNotificationContextImpl::DoReadNotificationData,
                  this, notification_id, origin, callback),
       base::Bind(callback, false /* success */, NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::DoReadNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const ReadResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  NotificationDatabaseData database_data;
  NotificationDatabase::Status status =
      database_->ReadNotificationData(notification_id,
                                      origin,
                                      &database_data);

  if (status == NotificationDatabase::STATUS_OK) {
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(callback,
                                       true /* success */,
                                       database_data));
    return;
  }

  // TODO(peter): Record UMA on |status| for reading from the database.
  // TODO(peter): Do the DeleteAndStartOver dance for STATUS_ERROR_CORRUPTED.

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, false /* success */, NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::WriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    const WriteResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
       base::Bind(&PlatformNotificationContextImpl::DoWriteNotificationData,
                  this, origin, database_data, callback),
       base::Bind(callback, false /* success */, 0 /* notification_id */));
}

void PlatformNotificationContextImpl::DoWriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    const WriteResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  int64_t notification_id = 0;
  NotificationDatabase::Status status =
      database_->WriteNotificationData(origin,
                                       database_data,
                                       &notification_id);

  DCHECK_GT(notification_id, 0);

  if (status == NotificationDatabase::STATUS_OK) {
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(callback,
                                       true /* success */,
                                       notification_id));
    return;
  }

  // TODO(peter): Record UMA on |status| for reading from the database.
  // TODO(peter): Do the DeleteAndStartOver dance for STATUS_ERROR_CORRUPTED.

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, false /* success */, 0 /* notification_id */));
}

void PlatformNotificationContextImpl::DeleteNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const DeleteResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
       base::Bind(&PlatformNotificationContextImpl::DoDeleteNotificationData,
                  this, notification_id, origin, callback),
       base::Bind(callback, false /* success */));
}

void PlatformNotificationContextImpl::DoDeleteNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const DeleteResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  NotificationDatabase::Status status =
      database_->DeleteNotificationData(notification_id, origin);

  const bool success = status == NotificationDatabase::STATUS_OK;

  // TODO(peter): Record UMA on |status| for reading from the database.
  // TODO(peter): Do the DeleteAndStartOver dance for STATUS_ERROR_CORRUPTED.

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback, success));
}

void PlatformNotificationContextImpl::LazyInitialize(
    const base::Closure& success_closure,
    const base::Closure& failure_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!task_runner_) {
    base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
    base::SequencedWorkerPool::SequenceToken token = pool->GetSequenceToken();

    task_runner_ = pool->GetSequencedTaskRunner(token);
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PlatformNotificationContextImpl::OpenDatabase,
                 this, success_closure, failure_closure));
}

void PlatformNotificationContextImpl::OpenDatabase(
    const base::Closure& success_closure,
    const base::Closure& failure_closure) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (database_) {
    success_closure.Run();
    return;
  }

  database_.reset(new NotificationDatabase(GetDatabasePath()));

  // TODO(peter): Record UMA on |status| for opening the database.
  // TODO(peter): Do the DeleteAndStartOver dance for STATUS_ERROR_CORRUPTED.

  NotificationDatabase::Status status =
      database_->Open(true /* create_if_missing */);

  if (status == NotificationDatabase::STATUS_OK) {
    success_closure.Run();
    return;
  }

  // TODO(peter): Properly handle failures when opening the database.
  database_.reset();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, failure_closure);
}

base::FilePath PlatformNotificationContextImpl::GetDatabasePath() const {
  if (path_.empty())
    return path_;

  return path_.Append(kPlatformNotificationsDirectory);
}

void PlatformNotificationContextImpl::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
