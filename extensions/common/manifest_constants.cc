// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace manifest_keys {

const char kAllFrames[] = "all_frames";
const char kAltKey[] = "altKey";
const char kApp[] = "app";
const char kAudio[] = "audio";
const char kBackgroundAllowJsAccess[] = "background.allow_js_access";
const char kBackgroundPage[] = "background.page";
const char kBackgroundPageLegacy[] = "background_page";
const char kBackgroundPersistent[] = "background.persistent";
const char kBackgroundScripts[] = "background.scripts";
const char kBrowserAction[] = "browser_action";
const char kChromeURLOverrides[] = "chrome_url_overrides";
const char kCommands[] = "commands";
const char kContentPack[] = "content_pack";
const char kContentPackSites[] = "sites";
const char kContentScripts[] = "content_scripts";
const char kContentSecurityPolicy[] = "content_security_policy";
const char kConvertedFromUserScript[] = "converted_from_user_script";
const char kCss[] = "css";
const char kCtrlKey[] = "ctrlKey";
const char kCurrentLocale[] = "current_locale";
const char kDefaultLocale[] = "default_locale";
const char kDescription[] = "description";
const char kDevToolsPage[] = "devtools_page";
const char kDisplayInLauncher[] = "display_in_launcher";
const char kDisplayInNewTabPage[] = "display_in_new_tab_page";
const char kEventName[] = "event_name";
const char kExcludeGlobs[] = "exclude_globs";
const char kExcludeMatches[] = "exclude_matches";
const char kExport[] = "export";
const char kExternallyConnectable[] = "externally_connectable";
const char kFileAccessList[] = "file_access";
const char kFileFilters[] = "file_filters";
const char kFileBrowserHandlers[] = "file_browser_handlers";
const char kMediaGalleriesHandlers[] = "media_galleries_handlers";
const char kFileHandlers[] = "file_handlers";
const char kFileHandlerExtensions[] = "extensions";
const char kFileHandlerTitle[] = "title";
const char kFileHandlerTypes[] = "types";
const char kHomepageURL[] = "homepage_url";
const char kIcons[] = "icons";
const char kId[] = "id";
const char kImport[] = "import";
const char kIncognito[] = "incognito";
const char kIncludeGlobs[] = "include_globs";
const char kInputComponents[] = "input_components";
const char kIsolation[] = "app.isolation";
const char kJs[] = "js";
const char kKey[] = "key";
const char kKeycode[] = "keyCode";
const char kKioskEnabled[] = "kiosk_enabled";
const char kLanguage[] = "language";
const char kLaunch[] = "app.launch";
const char kLaunchContainer[] = "app.launch.container";
const char kLaunchHeight[] = "app.launch.height";
const char kLaunchLocalPath[] = "app.launch.local_path";
const char kLaunchWebURL[] = "app.launch.web_url";
const char kLaunchWidth[] = "app.launch.width";
const char kLayouts[] = "layouts";
const char kManifestVersion[] = "manifest_version";
const char kMatches[] = "matches";
const char kMinimumChromeVersion[] = "minimum_chrome_version";
const char kMinimumVersion[] = "minimum_version";
const char kMIMETypes[] = "mime_types";
const char kMimeTypesHandler[] = "mime_types_handler";
const char kName[] = "name";
const char kNaClModules[] = "nacl_modules";
const char kNaClModulesMIMEType[] = "mime_type";
const char kNaClModulesPath[] = "path";
const char kOAuth2[] = "oauth2";
const char kOAuth2AutoApprove[] = "oauth2.auto_approve";
const char kOAuth2ClientId[] = "oauth2.client_id";
const char kOAuth2Scopes[] = "oauth2.scopes";
const char kOfflineEnabled[] = "offline_enabled";
const char kOmnibox[] = "omnibox";
const char kOmniboxKeyword[] = "omnibox.keyword";
const char kOptionalPermissions[] = "optional_permissions";
const char kOptionsPage[] = "options_page";
const char kPageAction[] = "page_action";
const char kPageActionDefaultIcon[] = "default_icon";
const char kPageActionDefaultPopup[] = "default_popup";
const char kPageActionDefaultTitle[] = "default_title";
const char kPageActionIcons[] = "icons";
const char kPageActionId[] = "id";
const char kPageActionPopup[] = "popup";
const char kPageActionPopupPath[] = "path";
const char kPageActions[] = "page_actions";
const char kPermissions[] = "permissions";
const char kPlatformAppBackground[] = "app.background";
const char kPlatformAppBackgroundPage[] = "app.background.page";
const char kPlatformAppBackgroundScripts[] = "app.background.scripts";
const char kPlatformAppContentSecurityPolicy[] = "app.content_security_policy";
const char kPlugins[] = "plugins";
const char kPluginsPath[] = "path";
const char kPluginsPublic[] = "public";
const char kPublicKey[] = "key";
const char kResources[] = "resources";
const char kRequirements[] = "requirements";
const char kRunAt[] = "run_at";
const char kSandboxedPages[] = "sandbox.pages";
const char kSandboxedPagesCSP[] = "sandbox.content_security_policy";
const char kScriptBadge[] = "script_badge";
const char kShiftKey[] = "shiftKey";
const char kShortcutKey[] = "shortcutKey";
const char kShortName[] = "short_name";
const char kSignature[] = "signature";
const char kSpellcheck[] = "spellcheck";
const char kSpellcheckDictionaryFormat[] = "dictionary_format";
const char kSpellcheckDictionaryLanguage[] = "dictionary_language";
const char kSpellcheckDictionaryLocale[] = "dictionary_locale";
const char kSpellcheckDictionaryPath[] = "dictionary_path";
const char kStorageManagedSchema[] = "storage.managed_schema";
const char kSuggestedKey[] = "suggested_key";
const char kSystemIndicator[] = "system_indicator";
const char kSystemInfoDisplay[] = "systemInfo.display";
const char kTheme[] = "theme";
const char kThemeColors[] = "colors";
const char kThemeDisplayProperties[] = "properties";
const char kThemeImages[] = "images";
const char kThemeTints[] = "tints";
const char kTtsEngine[] = "tts_engine";
const char kTtsGenderFemale[] = "female";
const char kTtsGenderMale[] = "male";
const char kTtsVoices[] = "voices";
const char kTtsVoicesEventTypeEnd[] = "end";
const char kTtsVoicesEventTypeError[] = "error";
const char kTtsVoicesEventTypeMarker[] = "marker";
const char kTtsVoicesEventTypeSentence[] = "sentence";
const char kTtsVoicesEventTypeStart[] = "start";
const char kTtsVoicesEventTypeWord[] = "word";
const char kTtsVoicesEventTypes[] = "event_types";
const char kTtsVoicesGender[] = "gender";
const char kTtsVoicesLang[] = "lang";
const char kTtsVoicesVoiceName[] = "voice_name";
const char kType[] = "type";
const char kUpdateURL[] = "update_url";
const char kUrlHandlers[] = "url_handlers";
const char kUrlHandlerTitle[] = "title";
const char kVersion[] = "version";
const char kWebAccessibleResources[] = "web_accessible_resources";
const char kWebURLs[] = "app.urls";

}  // namespace manifest_keys

namespace manifest_errors {

const char kPermissionUnknownOrMalformed[] =
    "Permission '*' is unknown or URL pattern is malformed.";
const char kUnrecognizedManifestKey[] = "Unrecognized manifest key '*'.";

}  // namespace manifest_errors

}  // namespace extensions
