// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_THREAD_OBSERVER_HELPER_H_
#define CHROME_TEST_BASE_THREAD_OBSERVER_HELPER_H_
#pragma once

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/test/notification_observer_mock.h"

// Helper class to add and remove observers on a non-UI thread from
// the UI thread.
template <class T, typename Traits>
class ThreadObserverHelper : public base::RefCountedThreadSafe<T, Traits> {
 public:
  explicit ThreadObserverHelper(content::BrowserThread::ID id)
      : id_(id), done_event_(false, false) {}

  void Init() {
    using content::BrowserThread;
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        id_,
        FROM_HERE,
        base::Bind(&ThreadObserverHelper::RegisterObserversTask, this));
    done_event_.Wait();
  }

  virtual ~ThreadObserverHelper() {
    DCHECK(content::BrowserThread::CurrentlyOn(id_));
    registrar_.RemoveAll();
  }

  content::NotificationObserverMock* observer() {
    return &observer_;
  }

 protected:
  friend class base::RefCountedThreadSafe<T>;

  virtual void RegisterObservers() = 0;

  content::NotificationRegistrar registrar_;
  content::NotificationObserverMock observer_;

 private:
  void RegisterObserversTask() {
    DCHECK(content::BrowserThread::CurrentlyOn(id_));
    RegisterObservers();
    done_event_.Signal();
  }

  content::BrowserThread::ID id_;
  base::WaitableEvent done_event_;
};

class DBThreadObserverHelper;
typedef ThreadObserverHelper<
    DBThreadObserverHelper,
    content::BrowserThread::DeleteOnDBThread> DBThreadObserverHelperBase;

class DBThreadObserverHelper : public DBThreadObserverHelperBase {
 public:
  DBThreadObserverHelper() :
      DBThreadObserverHelperBase(content::BrowserThread::DB) {}
};

#endif  // CHROME_TEST_BASE_THREAD_OBSERVER_HELPER_H_
