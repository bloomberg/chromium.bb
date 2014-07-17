// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_manifest_handlers.h"

#include "chrome/common/extensions/api/bluetooth/bluetooth_manifest_handler.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/extension_action/browser_action_handler.h"
#include "chrome/common/extensions/api/extension_action/page_action_handler.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#if defined(OS_CHROMEOS)
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#endif
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/api/spellcheck/spellcheck_handler.h"
#include "chrome/common/extensions/api/supervised_user_private/supervised_user_handler.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "chrome/common/extensions/manifest_handlers/mime_types_handler.h"
#include "chrome/common/extensions/manifest_handlers/minimum_chrome_version_checker.h"
#include "chrome/common/extensions/manifest_handlers/nacl_modules_handler.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/extensions/manifest_handlers/synthesize_browser_action_handler.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/common/api/sockets/sockets_manifest_handler.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/requirements_info.h"

namespace extensions {

void RegisterChromeManifestHandlers() {
  DCHECK(!ManifestHandler::IsRegistrationFinalized());
#if defined(ENABLE_EXTENSIONS)
  (new AboutPageHandler)->Register();
  (new AppIsolationHandler)->Register();
  (new AppLaunchManifestHandler)->Register();
  (new AutomationHandler)->Register();
  (new BluetoothManifestHandler)->Register();
  (new BrowserActionHandler)->Register();
  (new CommandsHandler)->Register();
  (new ContentScriptsHandler)->Register();
  (new DefaultLocaleHandler)->Register();
  (new DevToolsPageHandler)->Register();
  (new ExternallyConnectableHandler)->Register();
  (new FileBrowserHandlerParser)->Register();
  (new HomepageURLHandler)->Register();
#if defined(OS_CHROMEOS)
  (new InputComponentsHandler)->Register();
#endif
  (new MimeTypesHandlerParser)->Register();
  (new MinimumChromeVersionChecker)->Register();
#if !defined(DISABLE_NACL)
  (new NaClModulesHandler)->Register();
#endif
  (new OAuth2ManifestHandler)->Register();
  (new OmniboxHandler)->Register();
  (new OptionsPageHandler)->Register();
  (new PageActionHandler)->Register();
  (new PluginsHandler)->Register();
  (new RequirementsHandler)->Register();  // Depends on plugins.
  (new SettingsOverridesHandler)->Register();
  (new SocketsManifestHandler)->Register();
  (new SpellcheckHandler)->Register();
  (new StorageSchemaManifestHandler)->Register();
  (new SupervisedUserHandler)->Register();
  (new SynthesizeBrowserActionHandler)->Register();
  (new SystemIndicatorHandler)->Register();
  (new ThemeHandler)->Register();
  (new TtsEngineManifestHandler)->Register();
  (new UIOverridesHandler)->Register();
  (new UpdateURLHandler)->Register();
  (new UrlHandlersParser)->Register();
  (new URLOverridesHandler)->Register();
#endif
}

}  // namespace extensions
