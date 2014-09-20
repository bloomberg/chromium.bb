// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/common_manifest_handlers.h"

#include "extensions/common/api/sockets/sockets_manifest_handler.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/launcher_page_info.h"
#include "extensions/common/manifest_handlers/nacl_modules_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"

namespace extensions {

void RegisterCommonManifestHandlers() {
  DCHECK(!ManifestHandler::IsRegistrationFinalized());
#if defined(ENABLE_EXTENSIONS)
  (new BackgroundManifestHandler)->Register();
  (new CSPHandler(false))->Register();
  (new CSPHandler(true))->Register();
  (new ExternallyConnectableHandler)->Register();
  (new FileHandlersParser)->Register();
  (new IconsHandler)->Register();
  (new IncognitoHandler)->Register();
  (new KioskModeHandler)->Register();
  (new LauncherPageHandler)->Register();
#if !defined(DISABLE_NACL)
  (new NaClModulesHandler)->Register();
#endif
  (new OfflineEnabledHandler)->Register();
  (new SandboxedPageHandler)->Register();
  (new SharedModuleHandler)->Register();
  (new SocketsManifestHandler)->Register();
  (new WebAccessibleResourcesHandler)->Register();
  (new WebviewHandler)->Register();
#endif  // defined(ENABLE_EXTENSIONS)
}

}  // namespace extensions
