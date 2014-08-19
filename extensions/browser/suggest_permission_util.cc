// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/suggest_permission_util.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/common/console_message_level.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"

using content::CONSOLE_MESSAGE_LEVEL_WARNING;
using content::RenderViewHost;

namespace extensions {

namespace {

const char kPermissionsHelpURLForExtensions[] =
    "http://developer.chrome.com/extensions/manifest.html#permissions";
const char kPermissionsHelpURLForApps[] =
    "http://developer.chrome.com/apps/declare_permissions.html";

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

}  // namespace

bool IsExtensionWithPermissionOrSuggestInConsole(
    APIPermission::ID permission,
    const Extension* extension,
    content::RenderViewHost* host) {
  if (extension && extension->permissions_data()->HasAPIPermission(permission))
    return true;

  if (extension)
    SuggestAPIPermissionInDevToolsConsole(permission, extension, host);

  return false;
}

}  // namespace extensions
