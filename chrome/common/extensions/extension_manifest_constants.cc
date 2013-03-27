// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_manifest_constants.h"

namespace extension_manifest_keys {

const char kAllFrames[] = "all_frames";
const char kAltKey[] = "altKey";
const char kApp[] = "app";
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
const char kFileAccessList[] = "file_access";
const char kFileFilters[] = "file_filters";
const char kFileBrowserHandlers[] = "file_browser_handlers";
const char kMediaGalleriesHandlers[] = "media_galleries_handlers";
const char kFileHandlers[] = "file_handlers";
const char kFileHandlerTitle[] = "title";
const char kFileHandlerTypes[] = "types";
const char kHomepageURL[] = "homepage_url";
const char kIcons[] = "icons";
const char kId[] = "id";
const char kIncognito[] = "incognito";
const char kIncludeGlobs[] = "include_globs";
const char kInputComponents[] = "input_components";
const char kIntentDisposition[] = "disposition";
const char kIntentHref[] = "href";
const char kIntentPath[] = "path";
const char kIntents[] = "intents";
const char kIntentTitle[] = "title";
const char kIntentType[] = "type";
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
const char kLaunchMaxHeight[] = "app.launch.max_height";
const char kLaunchMaxWidth[] = "app.launch.max_width";
const char kLaunchMinHeight[] = "app.launch.min_height";
const char kLaunchMinWidth[] = "app.launch.min_width";
const char kLaunchWidth[] = "app.launch.width";
const char kLayouts[] = "layouts";
const char kManifestVersion[] = "manifest_version";
const char kMatches[] = "matches";
const char kMinimumChromeVersion[] = "minimum_chrome_version";
const char kMIMETypes[] = "mime_types";
const char kMimeTypesHandler[] = "mime_types_handler";
const char kName[] = "name";
const char kNaClModules[] = "nacl_modules";
const char kNaClModulesMIMEType[] = "mime_type";
const char kNaClModulesPath[] = "path";
const char kOAuth2[] = "oauth2";
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
const char kPageLauncher[] = "page_launcher";
const char kPermissions[] = "permissions";
const char kPlatformAppBackground[] = "app.background";
const char kPlatformAppBackgroundPage[] = "app.background.page";
const char kPlatformAppBackgroundScripts[] = "app.background.scripts";
const char kPlatformAppContentSecurityPolicy[] = "app.content_security_policy";
const char kPlugins[] = "plugins";
const char kPluginsPath[] = "path";
const char kPluginsPublic[] = "public";
const char kPublicKey[] = "key";
const char kRequirements[] = "requirements";
const char kRunAt[] = "run_at";
const char kSandboxedPages[] = "sandbox.pages";
const char kSandboxedPagesCSP[] = "sandbox.content_security_policy";
const char kScriptBadge[] = "script_badge";
const char kShiftKey[] = "shiftKey";
const char kShortcutKey[] = "shortcutKey";
const char kSignature[] = "signature";
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
const char kVersion[] = "version";
const char kWebAccessibleResources[] = "web_accessible_resources";
const char kWebURLs[] = "app.urls";
}  // namespace extension_manifest_keys

namespace extension_manifest_values {
const char kBrowserActionCommandEvent[] = "_execute_browser_action";
const char kIncognitoSplit[] = "split";
const char kIncognitoSpanning[] = "spanning";
const char kIntentDispositionWindow[] = "window";
const char kIntentDispositionInline[] = "inline";
const char kIsolatedStorage[] = "storage";
const char kKeybindingPlatformChromeOs[] = "chromeos";
const char kKeybindingPlatformDefault[] = "default";
const char kKeybindingPlatformLinux[] = "linux";
const char kKeybindingPlatformMac[] = "mac";
const char kKeybindingPlatformWin[] = "windows";
const char kRunAtDocumentStart[] = "document_start";
const char kRunAtDocumentEnd[] = "document_end";
const char kRunAtDocumentIdle[] = "document_idle";
const char kPageActionCommandEvent[] = "_execute_page_action";
const char kPageActionTypeTab[] = "tab";
const char kPageActionTypePermanent[] = "permanent";
const char kScriptBadgeCommandEvent[] = "_execute_script_badge";
const char kLaunchContainerPanel[] = "panel";
const char kLaunchContainerTab[] = "tab";
const char kLaunchContainerWindow[] = "window";
}  // namespace extension_manifest_values

// Extension-related error messages. Some of these are simple patterns, where a
// '*' is replaced at runtime with a specific value. This is used instead of
// printf because we want to unit test them and scanf is hard to make
// cross-platform.
namespace extension_manifest_errors {
const char kAppsNotEnabled[] =
    "Apps are not enabled.";
const char kBackgroundPermissionNeeded[] =
    "Hosted apps that use 'background_page' must have the 'background' "
    "permission.";
const char kBackgroundRequiredForPlatformApps[] =
    "Packaged apps must have a background page or background scripts.";
const char kCannotAccessPage[] =
    "Cannot access contents of url \"*\". "
    "Extension manifest must request permission to access this host.";
const char kCannotChangeExtensionID[] =
    "Installed extensions cannot change their IDs.";
const char kCannotClaimAllHostsInExtent[] =
    "Cannot claim all hosts ('*') in an extent.";
const char kCannotClaimAllURLsInExtent[] =
    "Cannot claim all URLs in an extent.";
const char kCannotScriptGallery[] =
    "The extensions gallery cannot be scripted.";
const char kChromeVersionTooLow[] =
    "This extension requires * version * or greater.";
const char kDisabledByPolicy[] =
    "This extension has been disabled by your administrator.";
const char kExpectString[] = "Expect string value.";
const char kExperimentalFlagRequired[] =
    "Loading extensions with 'experimental' permission is turned off by "
    "default. You can enable 'Experimental Extension APIs' "
    "by visiting chrome://flags.";
const char kInvalidAllFrames[] =
    "Invalid value for 'content_scripts[*].all_frames'.";
const char kInvalidBackground[] =
    "Invalid value for 'background_page'.";
const char kInvalidBackgroundAllowJsAccess[] =
    "Invalid value for 'background.allow_js_access'.";
const char kInvalidBackgroundCombination[] =
    "The background.page and background.scripts properties cannot be used at "
    "the same time.";
const char kInvalidBackgroundScript[] =
    "Invalid value for 'background.scripts[*]'.";
const char kInvalidBackgroundScripts[] =
    "Invalid value for 'background.scripts'.";
const char kInvalidBackgroundInHostedApp[] =
    "Invalid value for 'background_page'. Hosted apps must specify an "
    "absolute HTTPS URL for the background page.";
const char kInvalidBackgroundPersistent[] =
    "Invalid value for 'background.persistent'.";
const char kInvalidBackgroundPersistentNoPage[] =
    "Must specify one of background.page or background.scripts to use"
    " background.persistent.";
const char kInvalidBrowserAction[] =
    "Invalid value for 'browser_action'.";
const char kInvalidChromeURLOverrides[] =
    "Invalid value for 'chrome_url_overrides'.";
const char kInvalidCommandsKey[] =
    "Invalid value for 'commands'.";
const char kInvalidContentPack[] =
    "Invalid value for 'content_pack'.";
const char kInvalidContentPackSites[] =
    "Invalid value for Content Pack sites - files must be strings.";
const char kInvalidContentScript[] =
    "Invalid value for 'content_scripts[*]'.";
const char kInvalidContentSecurityPolicy[] =
    "Invalid value for 'content_security_policy'.";
const char kInvalidContentScriptsList[] =
    "Invalid value for 'content_scripts'.";
const char kInvalidCss[] =
    "Invalid value for 'content_scripts[*].css[*]'.";
const char kInvalidCssList[] =
    "Required value 'content_scripts[*].css' is invalid.";
const char kInvalidDefaultLocale[] =
    "Invalid value for default locale - locale name must be a string.";
const char kInvalidDescription[] =
    "Invalid value for 'description'.";
const char kInvalidDevToolsPage[] =
    "Invalid value for 'devtools_page'.";
const char kInvalidDisplayInLauncher[] =
    "Invalid value for 'display_in_launcher'.";
const char kInvalidDisplayInNewTabPage[] =
    "Invalid value for 'display_in_new_tab_page'.";
const char kInvalidExcludeMatch[] =
    "Invalid value for 'content_scripts[*].exclude_matches[*]': *";
const char kInvalidExcludeMatches[] =
    "Invalid value for 'content_scripts[*].exclude_matches'.";
const char kInvalidFileAccessList[] =
    "Invalid value for 'file_access'.";
const char kInvalidFileAccessValue[] =
    "Invalid value for 'file_access[*]'.";
const char kInvalidFileBrowserHandler[] =
    "Invalid value for 'file_browser_handlers'.";
const char kInvalidMediaGalleriesHandler[] =
    "Invalid value for 'media_galleries_handlers'.";
const char kInvalidFileFiltersList[] =
    "Invalid value for 'file_filters'.";
const char kInvalidFileFilterValue[] =
    "Invalid value for 'file_filters[*]'.";
const char kInvalidFileHandlers[] =
    "Invalid value for 'file_handlers'.";
const char kInvalidFileHandlerTitle[] =
    "Invalid value for 'file_handlers[*].title'.";
const char kInvalidFileHandlerType[] =
    "Invalid value for 'file_handlers[*].type'.";
const char kInvalidFileHandlerTypeElement[] =
    "Invalid value for 'file_handlers[*].type[*]'.";
const char kInvalidGlob[] =
    "Invalid value for 'content_scripts[*].*[*]'.";
const char kInvalidGlobList[] =
    "Invalid value for 'content_scripts[*].*'.";
const char kInvalidHomepageURL[] =
    "Invalid value for homepage url: '[*]'.";
const char kInvalidIconPath[] =
    "Invalid value for 'icons[\"*\"]'.";
const char kInvalidIcons[] =
    "Invalid value for 'icons'.";
const char kInvalidIncognitoBehavior[] =
    "Invalid value for 'incognito'.";
const char kInvalidIncognitoModeForPlatformApp[] =
    "Invalid value for 'incognito'. Packaged apps must use split incognito "
    "mode.";
const char kInvalidInputComponents[] =
    "Invalid value for 'input_components'";
const char kInvalidInputComponentDescription[] =
    "Invalid value for 'input_components[*].description";
const char kInvalidInputComponentLayoutName[] =
    "Invalid value for 'input_components[*].layouts[*]";
const char kInvalidInputComponentLayouts[] =
    "Invalid value for 'input_components[*].layouts";
const char kInvalidInputComponentName[] =
    "Invalid value for 'input_components[*].name";
const char kInvalidInputComponentShortcutKey[] =
    "Invalid value for 'input_components[*].shortcutKey";
const char kInvalidInputComponentShortcutKeycode[] =
    "Invalid value for 'input_components[*].shortcutKey.keyCode";
const char kInvalidInputComponentType[] =
    "Invalid value for 'input_components[*].type";
const char kInvalidIntent[] =
    "Invalid value for intents[*]";
const char kInvalidIntentDisposition[] =
    "Invalid value for intents[*].disposition";
const char kInvalidIntentDispositionInPlatformApp[] =
    "Invalid value for intents[*].disposition. Packaged apps cannot specify "
    "a disposition";
const char kInvalidIntentHref[] =
    "Invalid value for intents[*].href";
const char kInvalidIntentHrefEmpty[] =
    "Missing value for intents[*].href";
const char kInvalidIntentHrefInPlatformApp[] =
    "Invalid value for intents[*].href. Packaged apps cannot specify a "
    "URL for intents";
const char kInvalidIntentHrefOldAndNewKey[] =
    "intents[*]: Key \"*\" is deprecated.  Key \"*\" has the same meaning. "
    "You can not use both.";
const char kInvalidIntentPageInHostedApp[] =
    "Invalid value for intents[*].href. Hosted apps must specify an "
    "absolute URL within app.urls[].";
const char kInvalidIntents[] =
    "Invalid value for intents";
const char kInvalidIntentType[] =
    "Invalid value for intents[*].type";
const char kInvalidIntentTypeElement[] =
    "Invalid value for intents[*].type[*]";
const char kInvalidIntentTitle[] =
    "Invalid value for intents[*].title";
const char kInvalidIsolation[] =
    "Invalid value for 'app.isolation'.";
const char kInvalidIsolationValue[] =
    "Invalid value for 'app.isolation[*]'.";
const char kInvalidJs[] =
    "Invalid value for 'content_scripts[*].js[*]'.";
const char kInvalidJsList[] =
    "Required value 'content_scripts[*].js' is invalid.";
const char kInvalidKey[] =
    "Value 'key' is missing or invalid.";
const char kInvalidKeyBinding[] =
     "Invalid value for 'commands[*].*': *.";
const char kInvalidKeyBindingDescription[] =
    "Invalid value for 'commands[*].description'.";
const char kInvalidKeyBindingDictionary[] =
    "Contents of 'commands[*]' invalid.";
const char kInvalidKeyBindingMissingPlatform[] =
    "Could not find key specification for 'command[*].*': Either specify a key "
    "for '*', or specify a default key.";
const char kInvalidKeyBindingTooMany[] =
    "Too many shortcuts specified for 'commands': The maximum is *.";
const char kInvalidKeyBindingUnknownPlatform[] =
    "Unknown platform for 'command[*]': *. Valid values are: 'windows', 'mac'"
    " 'chromeos', 'linux' and 'default'.";
const char kInvalidKioskEnabled[] =
    "Invalid value for 'kiosk_enabled'.";
const char kInvalidLaunchContainer[] =
    "Invalid value for 'app.launch.container'.";
const char kInvalidLaunchValue[] =
    "Invalid value for '*'.";
const char kInvalidLaunchValueContainer[] =
    "Invalid container type for '*'.";
const char kInvalidManifest[] =
    "Manifest file is invalid.";
const char kInvalidManifestVersion[] =
    "Invalid value for 'manifest_version'. Must be an integer greater than "
    "zero.";
const char kInvalidManifestVersionOld[] =
    "The 'manifest_version' key must be present and set to * (without quotes). "
    "See developer.chrome.com/extensions/manifestVersion.html for details.";
const char kInvalidMatch[] =
    "Invalid value for 'content_scripts[*].matches[*]': *";
const char kInvalidMatchCount[] =
    "Invalid value for 'content_scripts[*].matches'. There must be at least "
    "one match specified.";
const char kInvalidMatches[] =
    "Required value 'content_scripts[*].matches' is missing or invalid.";
const char kInvalidMIMETypes[] =
    "Invalid value for 'mime_types'";
const char kInvalidMimeTypesHandler[] =
    "Invalid value for 'mime_types'.";
const char kInvalidMinimumChromeVersion[] =
    "Invalid value for 'minimum_chrome_version'.";
const char kInvalidName[] =
    "Required value 'name' is missing or invalid.";
const char kInvalidNaClModules[] =
    "Invalid value for 'nacl_modules'.";
const char kInvalidNaClModulesPath[] =
    "Invalid value for 'nacl_modules[*].path'.";
const char kInvalidNaClModulesMIMEType[] =
    "Invalid value for 'nacl_modules[*].mime_type'.";
const char kInvalidOAuth2ClientId[] =
    "Invalid value for 'oauth2.client_id'.";
const char kInvalidOAuth2Scopes[] =
    "Invalid value for 'oauth2.scopes'.";
const char kInvalidOfflineEnabled[] =
    "Invalid value for 'offline_enabled'.";
const char kInvalidOmniboxKeyword[] =
    "Invalid value for 'omnibox.keyword'.";
const char kInvalidOptionsPage[] =
    "Invalid value for 'options_page'.";
const char kInvalidOptionsPageExpectUrlInPackage[] =
    "Invalid value for 'options_page'.  Value must be a relative path.";
const char kInvalidOptionsPageInHostedApp[] =
    "Invalid value for 'options_page'. Hosted apps must specify an "
    "absolute URL.";
const char kInvalidPageAction[] =
    "Invalid value for 'page_action'.";
const char kInvalidPageActionDefaultTitle[] =
    "Invalid value for 'default_title'.";
const char kInvalidPageActionIconPath[] =
    "Invalid value for 'page_action.default_icon'.";
const char kInvalidPageActionId[] =
    "Required value 'id' is missing or invalid.";
const char kInvalidPageActionName[] =
    "Invalid value for 'page_action.name'.";
const char kInvalidPageActionOldAndNewKeys[] =
    "Key \"*\" is deprecated.  Key \"*\" has the same meaning.  You can not "
    "use both.";
const char kInvalidPageActionPopup[] =
    "Invalid type for page action popup.";
const char kInvalidPageActionPopupPath[] =
    "Invalid value for page action popup path [*].";
const char kInvalidPageActionsList[] =
    "Invalid value for 'page_actions'.";
const char kInvalidPageActionsListSize[] =
    "Invalid value for 'page_actions'. There can be at most one page action.";
const char kInvalidPageActionTypeValue[] =
    "Invalid value for 'page_actions[*].type', expected 'tab' or 'permanent'.";
const char kInvalidPageLauncher[] =
    "Invalid value for 'page_launcher'.";
const char kInvalidPermission[] =
    "Invalid value for 'permissions[*]'.";
const char kInvalidPermissions[] =
    "Required value 'permissions' is missing or invalid.";
const char kInvalidPermissionScheme[] =
    "Invalid scheme for 'permissions[*]'.";
const char kInvalidPlugins[] =
    "Invalid value for 'plugins'.";
const char kInvalidPluginsPath[] =
    "Invalid value for 'plugins[*].path'.";
const char kInvalidPluginsPublic[] =
    "Invalid value for 'plugins[*].public'.";
const char kInvalidRequirement[] =
    "Invalid value for requirement \"*\"";
const char kInvalidRequirements[] =
    "Invalid value for 'requirements'";
const char kInvalidRunAt[] =
    "Invalid value for 'content_scripts[*].run_at'.";
const char kInvalidSandboxedPagesList[] =
    "Invalid value for 'sandbox.pages'.";
const char kInvalidSandboxedPage[] =
    "Invalid value for 'sandbox.pages[*]'.";
const char kInvalidSandboxedPagesCSP[] =
    "Invalid value for 'sandbox.content_security_policy'.";
const char kInvalidScriptBadge[] =
    "Invalid value for 'script_badge'.";
const char kInvalidSignature[] =
    "Value 'signature' is missing or invalid.";
const char kInvalidSystemIndicator[] =
    "Invalid value for 'system_indicator'.";
const char kInvalidTheme[] =
    "Invalid value for 'theme'.";
const char kInvalidThemeColors[] =
    "Invalid value for theme colors - colors must be integers";
const char kInvalidThemeImages[] =
    "Invalid value for theme images - images must be strings.";
const char kInvalidThemeImagesMissing[] =
    "An image specified in the theme is missing.";
const char kInvalidThemeTints[] =
    "Invalid value for theme images - tints must be decimal numbers.";
const char kInvalidTts[] =
    "Invalid value for 'tts_engine'.";
const char kInvalidTtsVoices[] =
    "Invalid value for 'tts_engine.voices'.";
const char kInvalidTtsVoicesEventTypes[] =
    "Invalid value for 'tts_engine.voices[*].event_types'.";
const char kInvalidTtsVoicesGender[] =
    "Invalid value for 'tts_engine.voices[*].gender'.";
const char kInvalidTtsVoicesLang[] =
    "Invalid value for 'tts_engine.voices[*].lang'.";
const char kInvalidTtsVoicesVoiceName[] =
    "Invalid value for 'tts_engine.voices[*].voice_name'.";
const char kInvalidUpdateURL[] =
    "Invalid value for update url: '[*]'.";
const char kInvalidURLPatternError[] =
    "Invalid url pattern '*'";
const char kInvalidVersion[] =
    "Required value 'version' is missing or invalid. It must be between 1-4 "
    "dot-separated integers each between 0 and 65536.";
const char kInvalidWebAccessibleResourcesList[] =
    "Invalid value for 'web_accessible_resources'.";
const char kInvalidWebAccessibleResource[] =
    "Invalid value for 'web_accessible_resources[*]'.";
const char kInvalidWebURL[] =
    "Invalid value for 'app.urls[*]': *";
const char kInvalidWebURLs[] =
    "Invalid value for 'app.urls'.";
const char kInvalidZipHash[] =
    "Required key 'zip_hash' is missing or invalid.";
const char kInsecureContentSecurityPolicy[] =
    "Invalid value for 'content_security_policy': Both 'script-src' and"
    " 'object-src' directives must be specified (either explicitly, or"
    " implicitly via 'default-src'), and both must whitelist only secure"
    " resources. You may include any of the following sources: \"'self'\","
    " \"'unsafe-eval'\", \"http://127.0.0.1\", \"http://localhost\", or any"
    " \"https://\" or \"chrome-extension://\" origin. For more information,"
    " see http://developer.chrome.com/extensions/contentSecurityPolicy.html";
const char kLaunchPathAndExtentAreExclusive[] =
    "The 'app.launch.local_path' and 'app.urls' keys cannot both be set.";
const char kLaunchPathAndURLAreExclusive[] =
    "The 'app.launch.local_path' and 'app.launch.web_url' keys cannot "
    "both be set.";
const char kLaunchURLRequired[] =
    "Either 'app.launch.local_path' or 'app.launch.web_url' is required.";
const char kLocalesMessagesFileMissing[] =
    "Messages file is missing for locale.";
const char kLocalesNoDefaultLocaleSpecified[] =
    "Localization used, but default_locale wasn't specified in the manifest.";
const char kLocalesNoDefaultMessages[] =
    "Default locale is defined but default data couldn't be loaded.";
const char kLocalesNoValidLocaleNamesListed[] =
    "No valid locale name could be found in _locales directory.";
const char kLocalesTreeMissing[] =
    "Default locale was specified, but _locales subtree is missing.";
const char kManifestParseError[] =
    "Manifest is not valid JSON.";
const char kManifestUnreadable[] =
    "Manifest file is missing or unreadable.";
const char kMissingFile[] =
    "At least one js or css file is required for 'content_scripts[*]'.";
const char kMultipleOverrides[] =
    "An extension cannot override more than one page.";
const char kNoPermissionForMIMETypes[] =
    "The extension is not allowed to use mime_types key.";
const char kNoWildCardsInPaths[] =
  "Wildcards are not allowed in extent URL pattern paths.";
const char kOneUISurfaceOnly[] =
    "Only one of 'browser_action', 'page_action', and 'app' can be specified.";
const char kPermissionMustBeOptional[] =
    "Permission '*' must be specified in the optional section of the manifest.";
const char kPermissionNotAllowed[] =
    "Access to permission '*' denied.";
const char kPermissionNotAllowedInManifest[] =
    "Permission '*' cannot be specified in the manifest.";
const char kPlatformAppNeedsManifestVersion2[] =
    "Packaged apps need manifest_version set to >= 2";
const char kReservedMessageFound[] =
    "Reserved key * found in message catalog.";
const char kScriptBadgeRequiresFlag[] =
    "The script_badge manifest key is turned off by default. "
    "You can enable it with the --enable-script-badges command-line flag.";
const char kScriptBadgeIconIgnored[] =
    "default_icon specified in script_badge manifest section will not be used.";
const char kScriptBadgeTitleIgnored[] =
    "default_title specified in script_badge manifest section will not be "
    "used.";
const char kWebRequestConflictsWithLazyBackground[] =
    "The 'webRequest' API cannot be used with event pages.";
#if defined(OS_CHROMEOS)
const char kIllegalPlugins[] =
    "Extensions cannot install plugins on Chrome OS";
#endif
}  // namespace extension_manifest_errors
