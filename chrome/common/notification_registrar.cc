// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notification_registrar.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/common/notification_service.h"

struct NotificationRegistrar::Record {
  bool operator==(const Record& other) const;

  NotificationObserver* observer;
  NotificationType type;
  NotificationSource source;
};

bool NotificationRegistrar::Record::operator==(const Record& other) const {
  return observer == other.observer &&
         type == other.type &&
         source == other.source;
}

NotificationRegistrar::NotificationRegistrar() {
  if (!ChromeThread::GetCurrentThreadIdentifier(&thread_id_)) {
    // If we are not created in well known thread, GetCurrentThreadIdentifier
    // fails. Assign thread_id_ to an ID not used.
    thread_id_ = ChromeThread::ID_COUNT;
  }
}

NotificationRegistrar::~NotificationRegistrar() {
  RemoveAll();
}

void NotificationRegistrar::Add(NotificationObserver* observer,
                                NotificationType type,
                                const NotificationSource& source) {
  Record record = { observer, type, source };
  DCHECK(std::find(registered_.begin(), registered_.end(), record) ==
         registered_.end()) << "Duplicate registration.";
  registered_.push_back(record);

  if (ChromeThread::IsWellKnownThread(thread_id_)) {
    if (!ChromeThread::CurrentlyOn(thread_id_)) {
      // We are created on a well known thread, but this function is called
      // on a different thread. This could be a bug, or maybe the object is
      // passed around.
      // To be safe, reset thread_id_ so we don't call CHECK during remove.
      thread_id_ = ChromeThread::ID_COUNT;
    }
  }

  NotificationService::current()->AddObserver(observer, type, source);
}

void NotificationRegistrar::Remove(NotificationObserver* observer,
                                   NotificationType type,
                                   const NotificationSource& source) {
  Record record = { observer, type, source };
  RecordVector::iterator found = std::find(
      registered_.begin(), registered_.end(), record);
  if (found == registered_.end()) {
    NOTREACHED() << "Trying to remove unregistered observer of type " <<
        type.value << " from list of size " << registered_.size() << ".";
    return;
  }
  registered_.erase(found);

  CheckCalledOnValidWellKnownThread();

  // This can be NULL if our owner outlives the NotificationService, e.g. if our
  // owner is a Singleton.
  NotificationService* service = NotificationService::current();
  if (service)
    service->RemoveObserver(observer, type, source);
}

void NotificationRegistrar::RemoveAll() {
  // Early-exit if no registrations, to avoid calling
  // NotificationService::current.  If we've constructed an object with a
  // NotificationRegistrar member, but haven't actually used the notification
  // service, and we reach prgram exit, then calling current() below could try
  // to initialize the service's lazy TLS pointer during exit, which throws
  // wrenches at things.
  if (registered_.empty())
    return;

  CheckCalledOnValidWellKnownThread();

  // This can be NULL if our owner outlives the NotificationService, e.g. if our
  // owner is a Singleton.
  NotificationService* service = NotificationService::current();
  if (service) {
    for (size_t i = 0; i < registered_.size(); i++) {
      service->RemoveObserver(registered_[i].observer,
                              registered_[i].type,
                              registered_[i].source);
    }
  }
  registered_.clear();
}

bool NotificationRegistrar::IsEmpty() const {
  return registered_.empty();
}

void NotificationRegistrar::CheckCalledOnValidWellKnownThread() {
  if (ChromeThread::IsWellKnownThread(thread_id_)) {
    CHECK(ChromeThread::CurrentlyOn(thread_id_));
  }
}
