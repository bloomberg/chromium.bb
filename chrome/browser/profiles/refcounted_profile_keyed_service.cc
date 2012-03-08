// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"

namespace impl {

// static
void RefcountedProfileKeyedServiceTraits::Destruct(
    const RefcountedProfileKeyedService* obj) {
  if (obj->requires_destruction_on_thread_ &&
      !content::BrowserThread::CurrentlyOn(obj->thread_id_)) {
    content::BrowserThread::DeleteSoon(obj->thread_id_, FROM_HERE, obj);
  } else {
    delete obj;
  }
}

}  // namespace impl

RefcountedProfileKeyedService::RefcountedProfileKeyedService()
    : requires_destruction_on_thread_(false),
      thread_id_(content::BrowserThread::UI) {
}

RefcountedProfileKeyedService::RefcountedProfileKeyedService(
    const content::BrowserThread::ID thread_id)
    : requires_destruction_on_thread_(true),
      thread_id_(thread_id) {
}

RefcountedProfileKeyedService::~RefcountedProfileKeyedService() {}

