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
  ChromeThread::ID thread_id;
};

bool NotificationRegistrar::Record::operator==(const Record& other) const {
  return observer == other.observer &&
         type == other.type &&
         source == other.source;
  // thread_id is for debugging purpose and thus not compared here.
}

NotificationRegistrar::NotificationRegistrar() {
}

NotificationRegistrar::~NotificationRegistrar() {
  RemoveAll();
}

void NotificationRegistrar::Add(NotificationObserver* observer,
                                NotificationType type,
                                const NotificationSource& source) {
  Record record = { observer, type, source, GetCurrentThreadIdentifier() };

  DCHECK(std::find(registered_.begin(), registered_.end(), record) ==
         registered_.end()) << "Duplicate registration.";
  registered_.push_back(record);

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
  CheckCalledOnValidWellKnownThread(found->thread_id);

  registered_.erase(found);

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


  // This can be NULL if our owner outlives the NotificationService, e.g. if our
  // owner is a Singleton.
  NotificationService* service = NotificationService::current();
  if (service) {
    for (size_t i = 0; i < registered_.size(); i++) {
      CheckCalledOnValidWellKnownThread(registered_[i].thread_id);
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

ChromeThread::ID NotificationRegistrar::GetCurrentThreadIdentifier() {
  ChromeThread::ID thread_id;
  if (!ChromeThread::GetCurrentThreadIdentifier(&thread_id)) {
    // If we are not created in well known thread, GetCurrentThreadIdentifier
    // fails. Assign thread_id to an ID not used.
    thread_id = ChromeThread::ID_COUNT;
  }
  return thread_id;
}

void NotificationRegistrar::CheckCalledOnValidWellKnownThread(
    ChromeThread::ID thread_id) {
  if (ChromeThread::IsWellKnownThread(thread_id)) {
    CHECK(ChromeThread::CurrentlyOn(thread_id));
  }
}
