// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/refcounted_browser_context_keyed_service.h"

namespace impl {

// static
void RefcountedBrowserContextKeyedServiceTraits::Destruct(
    const RefcountedBrowserContextKeyedService* obj) {
  if (obj->requires_destruction_on_thread_ &&
      !content::BrowserThread::CurrentlyOn(obj->thread_id_)) {
    content::BrowserThread::DeleteSoon(obj->thread_id_, FROM_HERE, obj);
  } else {
    delete obj;
  }
}

}  // namespace impl

RefcountedBrowserContextKeyedService::RefcountedBrowserContextKeyedService()
    : requires_destruction_on_thread_(false),
      thread_id_(content::BrowserThread::UI) {}

RefcountedBrowserContextKeyedService::RefcountedBrowserContextKeyedService(
    const content::BrowserThread::ID thread_id)
    : requires_destruction_on_thread_(true), thread_id_(thread_id) {}

RefcountedBrowserContextKeyedService::~RefcountedBrowserContextKeyedService() {}
