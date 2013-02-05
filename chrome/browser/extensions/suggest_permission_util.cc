// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/suggest_permission_util.h"

#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/console_message_level.h"

using content::CONSOLE_MESSAGE_LEVEL_WARNING;
using content::RenderViewHost;

const char kPermissionsHelpURLForExtensions[] =
    "http://developer.chrome.com/extensions/manifest.html#permissions";
const char kPermissionsHelpURLForApps[] =
    "http://developer.chrome.com/apps/declare_permissions.html";

namespace extensions {

void SuggestAPIPermissionInDevToolsConsole(APIPermission::ID permission,
                                           const Extension* extension,
                                           content::RenderViewHost* host) {
  if (!extension || !host)
    return;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(permission);
  CHECK(permission_info);

  // Note, intentionally not internationalizing this string, as it is output
  // as a log message to developers in the developer tools console.
  std::string message = base::StringPrintf(
      "Is the '%s' permission appropriate? See %s.",
      permission_info->name(),
      extension->is_platform_app() ?
          kPermissionsHelpURLForApps : kPermissionsHelpURLForExtensions);

  host->Send(new ExtensionMsg_AddMessageToConsole(
      host->GetRoutingID(), CONSOLE_MESSAGE_LEVEL_WARNING, message));
}

void SuggestAPIPermissionInDevToolsConsole(APIPermission::ID permission,
                                           const Extension* extension,
                                           Profile* profile) {
  ExtensionProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();

  std::set<content::RenderViewHost*> views =
      process_manager->GetRenderViewHostsForExtension(extension->id());

  for (std::set<RenderViewHost*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    RenderViewHost* host = *iter;
    SuggestAPIPermissionInDevToolsConsole(permission, extension, host);
  }
}

bool IsExtensionWithPermissionOrSuggestInConsole(
    APIPermission::ID permission,
    const Extension* extension,
    content::RenderViewHost* host) {
  if (extension && extension->HasAPIPermission(permission))
    return true;

  if (extension)
    SuggestAPIPermissionInDevToolsConsole(permission, extension, host);

  return false;
}

bool IsExtensionWithPermissionOrSuggestInConsole(
    APIPermission::ID permission,
    const Extension* extension,
    Profile* profile) {
  if (extension && extension->HasAPIPermission(permission))
    return true;

  if (extension)
    SuggestAPIPermissionInDevToolsConsole(permission, extension, profile);

  return false;
}

} // namespace extensions
