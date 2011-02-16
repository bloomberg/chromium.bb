// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

namespace extension_manifest_keys {

const char* kAllFrames = "all_frames";
const char* kApp = "app";
const char* kBackground = "background_page";
const char* kBrowserAction = "browser_action";
const char* kChromeURLOverrides = "chrome_url_overrides";
const char* kContentScripts = "content_scripts";
const char* kConvertedFromUserScript = "converted_from_user_script";
const char* kCss = "css";
const char* kCurrentLocale = "current_locale";
const char* kDefaultLocale = "default_locale";
const char* kDescription = "description";
const char* kDevToolsPage = "devtools_page";
const char* kExcludeGlobs = "exclude_globs";
const char* kHomepageURL = "homepage_url";
const char* kIcons = "icons";
const char* kIncognito = "incognito";
const char* kIncludeGlobs = "include_globs";
const char* kJs = "js";
const char* kLaunch = "app.launch";
const char* kLaunchContainer = "app.launch.container";
const char* kLaunchHeight = "app.launch.height";
const char* kLaunchLocalPath = "app.launch.local_path";
const char* kLaunchWebURL = "app.launch.web_url";
const char* kLaunchWidth = "app.launch.width";
const char* kMatches = "matches";
const char* kMinimumChromeVersion = "minimum_chrome_version";
const char* kName = "name";
const char* kOmnibox = "omnibox";
const char* kOmniboxKeyword = "omnibox.keyword";
const char* kOptionsPage = "options_page";
const char* kPageAction = "page_action";
const char* kPageActionDefaultIcon = "default_icon";
const char* kPageActionDefaultPopup = "default_popup";
const char* kPageActionDefaultTitle = "default_title";
const char* kPageActionIcons = "icons";
const char* kPageActionId = "id";
const char* kPageActionPopup = "popup";
const char* kPageActionPopupHeight = "height";
const char* kPageActionPopupPath = "path";
const char* kPageActions = "page_actions";
const char* kPermissions = "permissions";
const char* kPlugins = "plugins";
const char* kPluginsPath = "path";
const char* kPluginsPublic = "public";
const char* kPublicKey = "key";
const char* kRunAt = "run_at";
const char* kSidebar = "sidebar";
const char* kSidebarDefaultIcon = "default_icon";
const char* kSidebarDefaultPage = "default_page";
const char* kSidebarDefaultTitle = "default_title";
const char* kSignature = "signature";
const char* kTheme = "theme";
const char* kThemeColors = "colors";
const char* kThemeDisplayProperties = "properties";
const char* kThemeImages = "images";
const char* kThemeTints = "tints";
const char* kToolstripPath = "path";
const char* kToolstrips = "toolstrips";
const char* kTts = "tts";
const char* kTtsGenderFemale = "female";
const char* kTtsGenderMale = "male";
const char* kTtsVoices = "voices";
const char* kTtsVoicesGender = "gender";
const char* kTtsVoicesLocale = "locale";
const char* kTtsVoicesVoiceName = "voiceName";
const char* kType = "type";
const char* kUpdateURL = "update_url";
const char* kVersion = "version";
const char* kWebURLs = "app.urls";
}  // namespace extension_manifest_keys

namespace extension_manifest_values {
const char* kIncognitoSplit = "split";
const char* kIncognitoSpanning = "spanning";
const char* kRunAtDocumentStart = "document_start";
const char* kRunAtDocumentEnd = "document_end";
const char* kRunAtDocumentIdle = "document_idle";
const char* kPageActionTypeTab = "tab";
const char* kPageActionTypePermanent = "permanent";
const char* kLaunchContainerPanel = "panel";
const char* kLaunchContainerTab = "tab";
const char* kLaunchContainerWindow = "window";
}  // namespace extension_manifest_values

// Extension-related error messages. Some of these are simple patterns, where a
// '*' is replaced at runtime with a specific value. This is used instead of
// printf because we want to unit test them and scanf is hard to make
// cross-platform.
namespace extension_manifest_errors {
const char* kAppsNotEnabled =
    "Apps are not enabled.";
const char* kCannotAccessPage =
    "Cannot access contents of url \"*\". "
    "Extension manifest must request permission to access this host.";
const char* kCannotScriptGallery =
    "The extensions gallery cannot be scripted.";
const char* kChromeVersionTooLow =
    "This extension requires * version * or greater.";
const char* kDisabledByPolicy =
    "This extension has been disabled by your administrator.";
const char* kDevToolsExperimental =
    "You must request the 'experimental' permission in order to use the"
    " DevTools API.";
const char* kExperimentalFlagRequired =
    "Loading extensions with 'experimental' permission requires"
    " --enable-experimental-extension-apis command line flag.";
const char* kHostedAppsCannotIncludeExtensionFeatures =
    "Hosted apps cannot use extension features.";
const char* kInvalidAllFrames =
    "Invalid value for 'content_scripts[*].all_frames'.";
const char* kInvalidBackground =
    "Invalid value for 'background_page'.";
const char* kInvalidBrowserAction =
    "Invalid value for 'browser_action'.";
const char* kInvalidChromeURLOverrides =
    "Invalid value for 'chrome_url_overrides'.";
const char* kInvalidContentScript =
    "Invalid value for 'content_scripts[*]'.";
const char* kInvalidContentScriptsList =
    "Invalid value for 'content_scripts'.";
const char* kInvalidCss =
    "Invalid value for 'content_scripts[*].css[*]'.";
const char* kInvalidCssList =
    "Required value 'content_scripts[*].css' is invalid.";
const char* kInvalidDefaultLocale =
    "Invalid value for default locale - locale name must be a string.";
const char* kInvalidDescription =
    "Invalid value for 'description'.";
const char* kInvalidDevToolsPage =
    "Invalid value for 'devtools_page'.";
const char* kInvalidGlob =
    "Invalid value for 'content_scripts[*].*[*]'.";
const char* kInvalidGlobList =
    "Invalid value for 'content_scripts[*].*'.";
const char* kInvalidHomepageURL =
    "Invalid value for homepage url: '[*]'.";
const char* kInvalidIconPath =
    "Invalid value for 'icons[\"*\"]'.";
const char* kInvalidIcons =
    "Invalid value for 'icons'.";
const char* kInvalidIncognitoBehavior =
    "Invalid value for 'incognito'.";
const char* kInvalidJs =
    "Invalid value for 'content_scripts[*].js[*]'.";
const char* kInvalidJsList =
    "Required value 'content_scripts[*].js' is invalid.";
const char* kInvalidKey =
    "Value 'key' is missing or invalid.";
const char* kInvalidLaunchContainer =
    "Invalid value for 'app.launch.container'.";
const char* kInvalidLaunchHeight =
    "Invalid value for 'app.launch.height'.";
const char* kInvalidLaunchHeightContainer =
    "Invalid container type for 'app.launch.height'.";
const char* kInvalidLaunchLocalPath =
    "Invalid value for 'app.launch.local_path'.";
const char* kInvalidLaunchWebURL =
    "Invalid value for 'app.launch.web_url'.";
const char* kInvalidLaunchWidth =
    "Invalid value for 'app.launch.width'.";
const char* kInvalidLaunchWidthContainer =
    "Invalid container type for 'app.launch.width'.";
const char* kInvalidManifest =
    "Manifest file is invalid.";
const char* kInvalidMatch =
    "Invalid value for 'content_scripts[*].matches[*]'.";
const char* kInvalidMatchCount =
    "Invalid value for 'content_scripts[*].matches'. There must be at least"
    "one match specified.";
const char* kInvalidMatches =
    "Required value 'content_scripts[*].matches' is missing or invalid.";
const char* kInvalidMinimumChromeVersion =
    "Invalid value for 'minimum_chrome_version'.";
const char* kInvalidName =
    "Required value 'name' is missing or invalid.";
const char* kInvalidOmniboxKeyword =
    "Invalid value for 'omnibox.keyword'.";
const char* kInvalidOptionsPage =
    "Invalid value for 'options_page'.";
const char* kInvalidOptionsPageExpectUrlInPackage =
    "Invalid value for 'options_page'.  Value must be a relative path.";
const char* kInvalidOptionsPageInHostedApp =
    "Invalid value for 'options_page'. Hosted apps must specify an "
    "absolute URL.";
const char* kInvalidPageAction =
    "Invalid value for 'page_action'.";
const char* kInvalidPageActionDefaultTitle =
    "Invalid value for 'default_title'.";
const char* kInvalidPageActionIconPath =
    "Invalid value for 'page_action.default_icon'.";
const char* kInvalidPageActionId =
    "Required value 'id' is missing or invalid.";
const char* kInvalidPageActionName =
    "Invalid value for 'page_action.name'.";
const char* kInvalidPageActionOldAndNewKeys =
    "Key \"*\" is deprecated.  Key \"*\" has the same meaning.  You can not "
    "use both.";
const char* kInvalidPageActionPopup =
    "Invalid type for page action popup.";
const char* kInvalidPageActionPopupHeight =
    "Invalid value for page action popup height [*].";
const char* kInvalidPageActionPopupPath =
    "Invalid value for page action popup path [*].";
const char* kInvalidPageActionsList =
    "Invalid value for 'page_actions'.";
const char* kInvalidPageActionsListSize =
    "Invalid value for 'page_actions'. There can be at most one page action.";
const char* kInvalidPageActionTypeValue =
    "Invalid value for 'page_actions[*].type', expected 'tab' or 'permanent'.";
const char* kInvalidPermission =
    "Invalid value for 'permissions[*]'.";
const char* kInvalidPermissions =
    "Required value 'permissions' is missing or invalid.";
const char* kInvalidPermissionScheme =
    "Invalid scheme for 'permissions[*]'.";
const char* kInvalidPlugins =
    "Invalid value for 'plugins'.";
const char* kInvalidPluginsPath =
    "Invalid value for 'plugins[*].path'.";
const char* kInvalidPluginsPublic =
    "Invalid value for 'plugins[*].public'.";
const char* kInvalidRunAt =
    "Invalid value for 'content_scripts[*].run_at'.";
const char* kInvalidSidebar =
    "Invalid value for 'sidebar'.";
const char* kInvalidSidebarDefaultIconPath =
    "Invalid value for 'sidebar.default_icon'.";
const char* kInvalidSidebarDefaultPage =
    "Invalid value for 'sidebar.default_page'.";
const char* kInvalidSidebarDefaultTitle =
    "Invalid value for 'sidebar.default_title'.";
const char* kInvalidSignature =
    "Value 'signature' is missing or invalid.";
const char* kInvalidTheme =
    "Invalid value for 'theme'.";
const char* kInvalidThemeColors =
    "Invalid value for theme colors - colors must be integers";
const char* kInvalidThemeImages =
    "Invalid value for theme images - images must be strings.";
const char* kInvalidThemeImagesMissing =
    "An image specified in the theme is missing.";
const char* kInvalidThemeTints =
    "Invalid value for theme images - tints must be decimal numbers.";
const char* kInvalidToolstrip =
    "Invalid value for 'toolstrips[*]'";
const char* kInvalidToolstrips =
    "Invalid value for 'toolstrips'.";
const char* kInvalidTts =
    "Invalid value for 'tts'.";
const char* kInvalidTtsVoices =
    "Invalid value for 'tts.voices'.";
const char* kInvalidTtsVoicesGender =
    "Invalid value for 'tts.voices[*].gender'.";
const char* kInvalidTtsVoicesLocale =
    "Invalid value for 'tts.voices[*].locale'.";
const char* kInvalidTtsVoicesVoiceName =
    "Invalid value for 'tts.voices[*].voiceName'.";
const char* kInvalidUpdateURL =
    "Invalid value for update url: '[*]'.";
const char* kInvalidVersion =
    "Required value 'version' is missing or invalid. It must be between 1-4 "
    "dot-separated integers each between 0 and 65536.";
const char* kInvalidWebURL =
    "Invalid value for 'app.urls[*]'.";
const char* kInvalidWebURLs =
    "Invalid value for 'app.urls'.";
const char* kInvalidZipHash =
    "Required key 'zip_hash' is missing or invalid.";
const char* kLaunchPathAndURLAreExclusive =
    "The 'app.launch.local_path' and 'launch.web_url' keys cannot both be set.";
const char* kLaunchURLRequired =
    "Either 'app.launch.local_path' or 'app.launch.web_url' is required.";
const char* kLocalesMessagesFileMissing =
    "Messages file is missing for locale.";
const char* kLocalesNoDefaultLocaleSpecified =
    "Localization used, but default_locale wasn't specified in the manifest.";
const char* kLocalesNoDefaultMessages =
    "Default locale is defined but default data couldn't be loaded.";
const char* kLocalesNoValidLocaleNamesListed =
    "No valid locale name could be found in _locales directory.";
const char* kLocalesTreeMissing =
    "Default locale was specified, but _locales subtree is missing.";
const char* kManifestParseError =
    "Manifest is not valid JSON.";
const char* kManifestUnreadable =
    "Manifest file is missing or unreadable.";
const char* kMissingFile =
    "At least one js or css file is required for 'content_scripts[*]'.";
const char* kMultipleOverrides =
    "An extension cannot override more than one page.";
const char* kOneUISurfaceOnly =
    "Only one of 'browser_action', 'page_action', and 'app' can be specified.";
const char* kReservedMessageFound =
    "Reserved key * found in message catalog.";
const char* kSidebarExperimental =
    "You must request the 'experimental' permission in order to use the"
    " Sidebar API.";
const char* kThemesCannotContainExtensions =
    "A theme cannot contain extensions code.";
#if defined(OS_CHROMEOS)
const char* kIllegalPlugins =
    "Extensions cannot install plugins on Chrome OS";
#endif
}  // namespace extension_manifest_errors

namespace extension_urls {
const char* kGalleryBrowsePrefix = "https://chrome.google.com/webstore";
const char* kMiniGalleryBrowsePrefix = "https://tools.google.com/chrome/";
const char* kMiniGalleryDownloadPrefix = "https://dl-ssl.google.com/chrome/";
}

namespace extension_filenames {
const char* kTempExtensionName = "CRX_INSTALL";

// The file to write our decoded images to, relative to the extension_path.
const char* kDecodedImagesFilename = "DECODED_IMAGES";

// The file to write our decoded message catalogs to, relative to the
// extension_path.
const char* kDecodedMessageCatalogsFilename = "DECODED_MESSAGE_CATALOGS";
}

namespace extension_misc {
const char* kBookmarkManagerId = "eemcgdkfndhakfknompkggombfjjjeno";
const char* kWebStoreAppId = "ahfgeienlihckogmohjhadlkjgocpleb";
const char* kAppsPromoHistogram = "Extensions.AppsPromo";
const char* kAppLaunchHistogram = "Extensions.AppLaunch";
#if defined(OS_CHROMEOS)
const char* kAccessExtensionPath =
    "/usr/share/chromeos-assets/accessibility/extensions";
#endif
}
