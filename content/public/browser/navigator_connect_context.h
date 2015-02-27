// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATOR_CONNECT_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATOR_CONNECT_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace content {

class NavigatorConnectServiceFactory;

// Use this class to register additional navigator.connect service factories.
// One instance of this class exists per StoragePartition.
// TODO(mek): Add API so services can close a connection.
class NavigatorConnectContext
    : public base::RefCountedThreadSafe<NavigatorConnectContext> {
 public:
  // Register a service factory. The most recently added factory that claims to
  // handle a URL will be used to handle a connection request for that URL.
  // This method must always be called on the UI thread. No guarantees are
  // given as to on what thread the passed in factory will get destroyed.
  virtual void AddFactory(
      scoped_ptr<NavigatorConnectServiceFactory> factory) = 0;

 protected:
  friend class base::RefCountedThreadSafe<NavigatorConnectContext>;
  NavigatorConnectContext() {}
  virtual ~NavigatorConnectContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATOR_CONNECT_CONTEXT_H_
