// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REQUEST_THROTTLER_REQUEST_THROTTLER_ENTRY_INTERFACE_H_
#define NET_REQUEST_THROTTLER_REQUEST_THROTTLER_ENTRY_INTERFACE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

class RequestThrottlerHeaderInterface;

// Represents an entry of the request throttler manager.
class RequestThrottlerEntryInterface
    : public base::RefCounted<RequestThrottlerEntryInterface> {
 public:
  RequestThrottlerEntryInterface() {}

  // This method needs to be called prior to every requests; if it returns
  // false, the calling module must cancel its current request.
  virtual bool IsRequestAllowed() const = 0;

  // This method needs to be called each time a response is received.
  virtual void UpdateWithResponse(
      const RequestThrottlerHeaderInterface* response) = 0;
 protected:
  virtual ~RequestThrottlerEntryInterface() {}
 private:
  friend class base::RefCounted<RequestThrottlerEntryInterface>;
  DISALLOW_COPY_AND_ASSIGN(RequestThrottlerEntryInterface);
};

#endif  // NET_REQUEST_THROTTLER_REQUEST_THROTTLER_ENTRY_INTERFACE_H_
