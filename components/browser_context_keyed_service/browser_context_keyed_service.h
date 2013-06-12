// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_CONTEXT_KEYED_SERVICE_BROWSER_CONTEXT_KEYED_SERVICE_H_
#define COMPONENTS_BROWSER_CONTEXT_KEYED_SERVICE_BROWSER_CONTEXT_KEYED_SERVICE_H_

#include "components/browser_context_keyed_service/browser_context_keyed_service_export.h"

class BrowserContextKeyedServiceFactory;

// Base class for all BrowserContextKeyedServices to allow for correct
// destruction order.
//
// Many services that hang off BrowserContext have a two-pass shutdown. Many
// subsystems need a first pass shutdown phase where they drop references. Not
// all services will need this, so there's a default implementation. Only once
// every system has been given a chance to drop references do we start deleting
// objects.
class BROWSER_CONTEXT_KEYED_SERVICE_EXPORT BrowserContextKeyedService {
 public:
  // The first pass is to call Shutdown on a BrowserContextKeyedService.
  virtual void Shutdown() {}

 protected:
  friend class BrowserContextKeyedServiceFactory;

  // The second pass is the actual deletion of each object.
  virtual ~BrowserContextKeyedService() {}
};

#endif  // COMPONENTS_BROWSER_CONTEXT_KEYED_SERVICE_BROWSER_CONTEXT_KEYED_SERVICE_H_
