// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"

namespace extensions {

APIResource::APIResource(APIResource::APIResourceType api_resource_type,
                         APIResourceEventNotifier* event_notifier)
    : api_resource_type_(api_resource_type), event_notifier_(event_notifier) {
  // scoped_refptr<> constructor does the initial AddRef() for us on
  // event_notifier_.
}

APIResource::~APIResource() {
  // scoped_refptr<> constructor calls Release() for us on event_notifier_.
}

APIResource::APIResourceType APIResource::api_resource_type() const {
  return api_resource_type_;
}

APIResourceEventNotifier* APIResource::event_notifier() const {
  return event_notifier_.get();
}

}
