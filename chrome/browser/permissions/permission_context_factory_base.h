// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_FACTORY_BASE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_FACTORY_BASE_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BrowserContentDependencyManager;

namespace content {
class BrowserContext;
}

// This base class goes along with the PermissionContextBase to be used for
// factories for PermissionContextBase's subclasses.
// - It regisers dependencies that PermissionContextBase uses.
// - It provides the common behaviour with incognito profiles.
class PermissionContextFactoryBase : public BrowserContextKeyedServiceFactory {
 protected:
  PermissionContextFactoryBase(const char* name,
                               BrowserContextDependencyManager* manager);
  ~PermissionContextFactoryBase() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_FACTORY_BASE_H_
