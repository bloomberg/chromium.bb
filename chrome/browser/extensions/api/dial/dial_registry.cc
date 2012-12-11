// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_registry.h"

#include "chrome/browser/extensions/api/dial/dial_api.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/common/extensions/api/dial.h"

namespace extensions {

DialRegistry::DialRegistry(Observer* dial_api)
  : num_listeners_(0), discovering_(false), dial_api_(dial_api) { }

DialRegistry::~DialRegistry() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DialRegistry::OnListenerAdded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (++num_listeners_ == 1) {
    DVLOG(1) << "Listener added; starting periodic discovery.";
    discovering_ = true;
  }
}

void DialRegistry::OnListenerRemoved() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (--num_listeners_ == 0) {
    DVLOG(1) << "Listeners removed; stopping periodic discovery.";
    discovering_ = false;
  }
}

bool DialRegistry::DiscoverNow() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return discovering_;
}

}  // namespace extensions
