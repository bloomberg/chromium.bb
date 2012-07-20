// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"

namespace extensions {

ApiResource::ApiResource(ApiResourceEventNotifier* event_notifier)
    : event_notifier_(event_notifier) {
  // scoped_refptr<> constructor does the initial AddRef() for us on
  // event_notifier_.
}

ApiResource::~ApiResource() {
  // scoped_refptr<> constructor calls Release() for us on event_notifier_.
}

ApiResourceEventNotifier* ApiResource::event_notifier() const {
  return event_notifier_.get();
}

}
