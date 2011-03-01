// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_THREAD_OBSERVER_HELPER_H__
#define CHROME_TEST_THREAD_OBSERVER_HELPER_H__
#pragma once

#include "base/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"

// Helper class to add and remove observers on a non-UI thread from
// the UI thread.
template <class T, typename Traits>
class ThreadObserverHelper : public base::RefCountedThreadSafe<T, Traits> {
 public:
  explicit ThreadObserverHelper(BrowserThread::ID id)
      : id_(id), done_event_(false, false) {}

  void Init() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        id_,
        FROM_HERE,
        NewRunnableMethod(this, &ThreadObserverHelper::RegisterObserversTask));
    done_event_.Wait();
  }

  virtual ~ThreadObserverHelper() {
    DCHECK(BrowserThread::CurrentlyOn(id_));
    registrar_.RemoveAll();
  }

  NotificationObserverMock* observer() {
    return &observer_;
  }

 protected:
  friend class base::RefCountedThreadSafe<T>;

  virtual void RegisterObservers() = 0;

  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;

 private:
  void RegisterObserversTask() {
    DCHECK(BrowserThread::CurrentlyOn(id_));
    RegisterObservers();
    done_event_.Signal();
  }

  BrowserThread::ID id_;
  base::WaitableEvent done_event_;
};

class DBThreadObserverHelper;
typedef ThreadObserverHelper<
    DBThreadObserverHelper,
    BrowserThread::DeleteOnDBThread> DBThreadObserverHelperBase;

class DBThreadObserverHelper : public DBThreadObserverHelperBase {
 public:
  DBThreadObserverHelper() : DBThreadObserverHelperBase(BrowserThread::DB) {}
};

#endif  // CHROME_TEST_THREAD_OBSERVER_HELPER_H__
