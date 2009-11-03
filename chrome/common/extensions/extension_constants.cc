// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

namespace extension_manifest_keys {

const wchar_t* kBackground = L"background_page";
const wchar_t* kBrowserAction = L"browser_action";
const wchar_t* kChromeURLOverrides = L"chrome_url_overrides";
const wchar_t* kContentScripts = L"content_scripts";
const wchar_t* kCss = L"css";
const wchar_t* kDefaultLocale = L"default_locale";
const wchar_t* kDescription = L"description";
const wchar_t* kIcons = L"icons";
const wchar_t* kJs = L"js";
const wchar_t* kMatches = L"matches";
const wchar_t* kName = L"name";
const wchar_t* kPageActionId = L"id";
const wchar_t* kPageAction = L"page_action";
const wchar_t* kPageActions = L"page_actions";
const wchar_t* kPageActionIcons = L"icons";
const wchar_t* kPageActionDefaultIcon = L"default_icon";
const wchar_t* kPageActionDefaultTitle = L"default_title";
const wchar_t* kPageActionPopup = L"popup";
const wchar_t* kPageActionPopupHeight = L"height";
const wchar_t* kPageActionPopupPath = L"path";
const wchar_t* kPermissions = L"permissions";
const wchar_t* kPlugins = L"plugins";
const wchar_t* kPluginsPath = L"path";
const wchar_t* kPluginsPublic = L"public";
const wchar_t* kPrivacyBlacklists = L"privacy_blacklists";
const wchar_t* kPublicKey = L"key";
const wchar_t* kRunAt = L"run_at";
const wchar_t* kSignature = L"signature";
const wchar_t* kTheme = L"theme";
const wchar_t* kThemeImages = L"images";
const wchar_t* kThemeColors = L"colors";
const wchar_t* kThemeTints = L"tints";
const wchar_t* kThemeDisplayProperties = L"properties";
const wchar_t* kToolstripMoleHeight = L"mole_height";
const wchar_t* kToolstripMolePath = L"mole";
const wchar_t* kToolstripPath = L"path";
const wchar_t* kToolstrips = L"toolstrips";
const wchar_t* kType = L"type";
const wchar_t* kVersion = L"version";
const wchar_t* kUpdateURL = L"update_url";
const wchar_t* kOptionsPage = L"options_page";
}  // namespace extension_manifest_keys

namespace extension_manifest_values {
const char* kRunAtDocumentStart = "document_start";
const char* kRunAtDocumentEnd = "document_end";
const char* kPageActionTypeTab = "tab";
const char* kPageActionTypePermanent = "permanent";
}  // namespace extension_manifest_values

// Extension-related error messages. Some of these are simple patterns, where a
// '*' is replaced at runtime with a specific value. This is used instead of
// printf because we want to unit test them and scanf is hard to make
// cross-platform.
namespace extension_manifest_errors {
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
    "Required value 'content_scripts[*].css is invalid.";
const char* kInvalidDescription =
    "Invalid value for 'description'.";
const char* kInvalidIcons =
    "Invalid value for 'icons'.";
const char* kInvalidIconPath =
    "Invalid value for 'icons[\"*\"]'.";
const char* kInvalidJs =
    "Invalid value for 'content_scripts[*].js[*]'.";
const char* kInvalidJsList =
    "Required value 'content_scripts[*].js is invalid.";
const char* kInvalidKey =
    "Value 'key' is missing or invalid.";
const char* kInvalidManifest =
    "Manifest file is invalid.";
const char* kInvalidMatchCount =
    "Invalid value for 'content_scripts[*].matches. There must be at least one "
    "match specified.";
const char* kInvalidMatch =
    "Invalid value for 'content_scripts[*].matches[*]'.";
const char* kInvalidMatches =
    "Required value 'content_scripts[*].matches' is missing or invalid.";
const char* kInvalidName =
    "Required value 'name' is missing or invalid.";
const char* kInvalidPageAction =
    "Invalid value for 'page_action'.";
const char* kInvalidPageActionName =
    "Invalid value for 'page_action.name'.";
const char* kInvalidPageActionIconPath =
    "Invalid value for 'page_action.default_icon'.";
const char* kInvalidPageActionsList =
    "Invalid value for 'page_actions'.";
const char* kInvalidPageActionsListSize =
    "Invalid value for 'page_actions'. There can be only one.";
const char* kInvalidPageActionId =
    "Required value 'id' is missing or invalid.";
const char* kInvalidPageActionDefaultTitle =
    "Invalid value for 'default_title'.";
const char* kInvalidPageActionPopup =
    "Invalid type for page action popup.";
const char* kInvalidPageActionPopupHeight =
    "Invalid value for page action popup height [*].";
const char* kInvalidPageActionPopupPath =
    "Invalid value for page action popup path [*].";
const char* kInvalidPageActionTypeValue =
    "Invalid value for 'page_actions[*].type', expected 'tab' or 'permanent'.";
const char* kInvalidPermissions =
    "Required value 'permissions' is missing or invalid.";
const char* kInvalidPermission =
    "Invalid value for 'permissions[*]'.";
const char* kInvalidPermissionScheme =
    "Invalid scheme for 'permissions[*]'. Only 'http' and 'https' are "
    "allowed.";
const char* kInvalidPlugins =
    "Invalid value for 'plugins'.";
const char* kInvalidPluginsPath =
    "Invalid value for 'plugins[*].path'.";
const char* kInvalidPluginsPublic =
    "Invalid value for 'plugins[*].public'.";
const char* kInvalidPrivacyBlacklists =
    "Invalid value for 'privacy_blacklists'.";
const char* kInvalidPrivacyBlacklistsPath =
    "Invalid value for 'privacy_blacklists[*]'.";
const char* kInvalidBackground =
    "Invalid value for 'background_page'.";
const char* kInvalidRunAt =
    "Invalid value for 'content_scripts[*].run_at'.";
const char* kInvalidSignature =
    "Value 'signature' is missing or invalid.";
const char* kInvalidToolstrip =
    "Invalid value for 'toolstrips[*]'";
const char* kInvalidToolstrips =
    "Invalid value for 'toolstrips'.";
const char* kInvalidVersion =
    "Required value 'version' is missing or invalid. It must be between 1-4 "
    "dot-separated integers.";
const char* kInvalidZipHash =
    "Required key 'zip_hash' is missing or invalid.";
const char* kManifestParseError =
    "Manifest is not valid JSON.";
const char* kManifestUnreadable =
    "Manifest file is missing or unreadable.";
const char* kMissingFile =
    "At least one js or css file is required for 'content_scripts[*]'.";
const char* kInvalidTheme =
    "Invalid value for 'theme'.";
const char* kInvalidThemeImages =
    "Invalid value for theme images - images must be strings.";
const char* kInvalidThemeImagesMissing =
    "Am image specified in the theme is missing.";
const char* kInvalidThemeColors =
    "Invalid value for theme colors - colors must be integers";
const char* kInvalidThemeTints =
    "Invalid value for theme images - tints must be decimal numbers.";
const char* kInvalidUpdateURL =
    "Invalid value for update url: '[*]'.";
const char* kInvalidDefaultLocale =
    "Invalid value for default locale - locale name must be a string.";
const char* kOneUISurfaceOnly =
    "An extension cannot have both a page action and a browser action.";
const char* kThemesCannotContainExtensions =
    "A theme cannot contain extensions code.";
const char* kLocalesNoDefaultLocaleSpecified =
    "Localization used, but default_locale wasn't specified in the manifest.";
const char* kLocalesNoValidLocaleNamesListed =
    "No valid locale name could be found in _locales directory.";
const char* kInvalidOptionsPage =
    "Invalid value for 'options_page'.";
}  // namespace extension_manifest_errors
