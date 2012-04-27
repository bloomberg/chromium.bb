// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace extensions {

class APIResourceController;
class APIResourceEventNotifier;

// An APIResource represents something that an extension API manages, such as a
// socket or a serial-port connection.
class APIResource {
 public:
  virtual ~APIResource();

 protected:
  enum APIResourceType {
    SocketResource,
    SerialConnectionResource,
    UsbDeviceResource,
  };
  APIResource(APIResourceType api_resource_type,
              APIResourceEventNotifier* event_notifier);

  APIResourceType api_resource_type() const;
  APIResourceEventNotifier* event_notifier() const;

 private:
  APIResourceType api_resource_type_;
  scoped_refptr<APIResourceEventNotifier> event_notifier_;

  friend class APIResourceController;
  DISALLOW_COPY_AND_ASSIGN(APIResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_H_
