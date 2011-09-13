// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_LOGIN_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_LOGIN_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

// Interface for getting login credentials for HTTP auth requests.  When the
// implementation has the credentials, it shoudl call the Requests's SetAuth
// method.
class CONTENT_EXPORT ResourceDispatcherHostLoginDelegate
    : public base::RefCountedThreadSafe<ResourceDispatcherHostLoginDelegate> {
 public:
  ResourceDispatcherHostLoginDelegate();
  virtual ~ResourceDispatcherHostLoginDelegate();

  // Notify the delegate that the request was cancelled.
  // This function can only be called from the IO thread.
  virtual void OnRequestCancelled();
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_LOGIN_DELEGATE_H_
