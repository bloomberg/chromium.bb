// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/keep_alive_service_impl.h"

#include "chrome/browser/lifetime/application_lifetime.h"

ScopedKeepAlive::ScopedKeepAlive() {
  chrome::StartKeepAlive();
}

ScopedKeepAlive::~ScopedKeepAlive() {
  chrome::EndKeepAlive();
}

KeepAliveServiceImpl::KeepAliveServiceImpl() {
}

KeepAliveServiceImpl::~KeepAliveServiceImpl() {
}

void KeepAliveServiceImpl::EnsureKeepAlive() {
  if (!keep_alive_)
    keep_alive_.reset(new ScopedKeepAlive());
}

void KeepAliveServiceImpl::FreeKeepAlive() {
  if (keep_alive_)
    keep_alive_.reset();
}
