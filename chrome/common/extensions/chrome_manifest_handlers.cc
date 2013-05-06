// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_manifest_handlers.h"

#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/extension_action/browser_action_handler.h"
#include "chrome/common/extensions/api/extension_action/page_action_handler.h"
#include "chrome/common/extensions/api/extension_action/script_badge_handler.h"
#include "chrome/common/extensions/api/file_handlers/file_handlers_parser.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#if defined(OS_CHROMEOS)
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#endif
#include "chrome/common/extensions/api/managed_mode_private/managed_mode_handler.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/api/page_launcher/page_launcher_handler.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/api/spellcheck/spellcheck_handler.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/csp_handler.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "chrome/common/extensions/manifest_handlers/externally_connectable.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/extensions/manifest_handlers/kiosk_enabled_info.h"
#include "chrome/common/extensions/manifest_handlers/offline_enabled_info.h"
#include "chrome/common/extensions/manifest_handlers/requirements_handler.h"
#include "chrome/common/extensions/manifest_handlers/sandboxed_page_info.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/mime_types_handler.h"
#include "chrome/common/extensions/web_accessible_resources_handler.h"

namespace extensions {

void RegisterChromeManifestHandlers() {
#if defined(ENABLE_EXTENSIONS)
  (new AppIsolationHandler)->Register();
  (new BackgroundManifestHandler)->Register();
  (new BrowserActionHandler)->Register();
  (new CommandsHandler)->Register();
  (new ContentScriptsHandler)->Register();
  (new CSPHandler(false))->Register();
  (new CSPHandler(true))->Register();
  (new DefaultLocaleHandler)->Register();
  (new DevToolsPageHandler)->Register();
  (new ExternallyConnectableHandler)->Register();
  (new FileHandlersParser)->Register();
  (new HomepageURLHandler)->Register();
  (new IconsHandler)->Register();
  (new IncognitoHandler)->Register();
#if defined(OS_CHROMEOS)
  (new InputComponentsHandler)->Register();
#endif
  (new KioskEnabledHandler)->Register();
  (new ManagedModeHandler)->Register();
  (new MimeTypesHandlerParser)->Register();
  (new OAuth2ManifestHandler)->Register();
  (new OfflineEnabledHandler)->Register();
  (new OmniboxHandler)->Register();
  (new OptionsPageHandler)->Register();
  (new PageActionHandler)->Register();
  (new PageLauncherHandler)->Register();
  (new PluginsHandler)->Register();
  (new RequirementsHandler)->Register();
  (new SandboxedPageHandler)->Register();
  (new ScriptBadgeHandler)->Register();
  (new SharedModuleHandler)->Register();
  (new SpellcheckHandler)->Register();
  (new SystemIndicatorHandler)->Register();
  (new ThemeHandler)->Register();
  (new TtsEngineManifestHandler)->Register();
  (new UpdateURLHandler)->Register();
  (new URLOverridesHandler)->Register();
  (new WebAccessibleResourcesHandler)->Register();
#endif
}

}  // namespace extensions
