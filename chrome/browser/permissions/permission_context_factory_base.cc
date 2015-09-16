// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context_factory_base.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

PermissionContextFactoryBase::PermissionContextFactoryBase(
    const char* name,
    BrowserContextDependencyManager* manager)
    : BrowserContextKeyedServiceFactory(name, manager) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionContextFactoryBase::~PermissionContextFactoryBase() {
}

content::BrowserContext*
PermissionContextFactoryBase::GetBrowserContextToUse(
      content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
