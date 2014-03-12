// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "content/public/browser/browser_thread.h"

class RefcountedBrowserContextKeyedService;

namespace impl {

struct KEYED_SERVICE_EXPORT RefcountedBrowserContextKeyedServiceTraits {
  static void Destruct(const RefcountedBrowserContextKeyedService* obj);
};

}  // namespace impl

// Base class for refcounted objects that hang off the BrowserContext.
//
// The two pass shutdown described in KeyedService works a bit differently
// because there could be outstanding references on other threads.
// ShutdownOnUIThread() will be called on the UI thread, and then the
// destructor will run when the last reference is dropped, which may or may not
// be after the corresponding BrowserContext has been destroyed.
//
// Optionally, if you initialize your service with the constructor that takes a
// thread ID, your service will be deleted on that thread. We can't use
// content::DeleteOnThread<> directly because
// RefcountedBrowserContextKeyedService must be one type that
// RefcountedBrowserContextKeyedServiceFactory can use.
class KEYED_SERVICE_EXPORT RefcountedBrowserContextKeyedService
    : public base::RefCountedThreadSafe<
          RefcountedBrowserContextKeyedService,
          impl::RefcountedBrowserContextKeyedServiceTraits> {
 public:
  // Unlike KeyedService, ShutdownOnUI is not optional. You must do something
  // to drop references during the first pass Shutdown() because this is the
  // only point where you are guaranteed that something is running on the UI
  // thread. The PKSF framework will ensure that this is only called on the UI
  // thread; you do not need to check for that yourself.
  virtual void ShutdownOnUIThread() = 0;

 protected:
  // If your service does not need to be deleted on a specific thread, use the
  // default constructor.
  RefcountedBrowserContextKeyedService();

  // If you need your service to be deleted on a specific thread (for example,
  // you're converting a service that used content::DeleteOnThread<IO>), then
  // use this constructor with the ID of the thread.
  explicit RefcountedBrowserContextKeyedService(
      const content::BrowserThread::ID thread_id);

  // The second pass destruction can happen anywhere unless you specify which
  // thread this service must be destroyed on by using the second constructor.
  virtual ~RefcountedBrowserContextKeyedService();

 private:
  friend struct impl::RefcountedBrowserContextKeyedServiceTraits;
  friend class base::DeleteHelper<RefcountedBrowserContextKeyedService>;
  friend class base::RefCountedThreadSafe<
      RefcountedBrowserContextKeyedService,
      impl::RefcountedBrowserContextKeyedServiceTraits>;

  // Do we have to delete this object on a specific thread?
  bool requires_destruction_on_thread_;
  content::BrowserThread::ID thread_id_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_H_
