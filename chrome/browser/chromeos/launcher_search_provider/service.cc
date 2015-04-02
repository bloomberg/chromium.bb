// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search_provider/service.h"

#include "chrome/browser/chromeos/launcher_search_provider/service_factory.h"
#include "chrome/common/extensions/api/launcher_search_provider.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace api_launcher_search_provider =
    extensions::api::launcher_search_provider;
using extensions::ExtensionId;
using extensions::ExtensionSet;

namespace chromeos {
namespace launcher_search_provider {

Service::Service(Profile* profile,
                 extensions::ExtensionRegistry* extension_registry)
    : profile_(profile), extension_registry_(extension_registry), query_id_(0) {
}

Service::~Service() {
}

// static
Service* Service::Get(content::BrowserContext* context) {
  return ServiceFactory::Get(context);
}

void Service::OnQueryStarted(const std::string& query, const int max_result) {
  ++query_id_;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  std::set<ExtensionId> extension_ids = GetListenerExtensionIds();
  for (const ExtensionId extension_id : extension_ids) {
    event_router->DispatchEventToExtension(
        extension_id,
        make_scoped_ptr(new extensions::Event(
            api_launcher_search_provider::OnQueryStarted::kEventName,
            api_launcher_search_provider::OnQueryStarted::Create(
                std::to_string(query_id_), query, max_result))));
  }
}

std::set<ExtensionId> Service::GetListenerExtensionIds() {
  std::set<ExtensionId> extension_ids;

  const ExtensionSet& extension_set = extension_registry_->enabled_extensions();
  for (scoped_refptr<const extensions::Extension> extension : extension_set) {
    const extensions::PermissionsData* permission_data =
        extension->permissions_data();
    const bool has_permission = permission_data->HasAPIPermission(
        extensions::APIPermission::kLauncherSearchProvider);
    if (has_permission)
      extension_ids.insert(extension->id());
  }

  return extension_ids;
}

}  // namespace launcher_search_provider
}  // namespace chromeos
