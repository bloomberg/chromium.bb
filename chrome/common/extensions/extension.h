// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

class DictionaryValue;
class ExtensionAction;
class ExtensionResource;
class ExtensionSidebarDefaults;
class SkBitmap;
class Version;

// Represents a Chrome extension.
class Extension : public base::RefCountedThreadSafe<Extension> {
 public:
  typedef std::map<const std::string, GURL> URLOverrideMap;
  typedef std::vector<std::string> ScriptingWhitelist;

  // What an extension was loaded from.
  // NOTE: These values are stored as integers in the preferences and used
  // in histograms so don't remove or reorder existing items.  Just append
  // to the end.
  enum Location {
    INVALID,
    INTERNAL,           // A crx file from the internal Extensions directory.
    EXTERNAL_PREF,      // A crx file from an external directory (via prefs).
    EXTERNAL_REGISTRY,  // A crx file from an external directory (via eg the
                        // registry on Windows).
    LOAD,               // --load-extension.
    COMPONENT,          // An integral component of Chrome itself, which
                        // happens to be implemented as an extension. We don't
                        // show these in the management UI.
    EXTERNAL_PREF_DOWNLOAD,    // A crx file from an external directory (via
                               // prefs), installed from an update URL.
    EXTERNAL_POLICY_DOWNLOAD,  // A crx file from an external directory (via
                               // admin policies), installed from an update URL.

    NUM_LOCATIONS
  };

  enum State {
    DISABLED = 0,
    ENABLED,
    KILLBIT,  // Don't install/upgrade (applies to external extensions only).

    NUM_STATES
  };

  enum InstallType {
    INSTALL_ERROR,
    DOWNGRADE,
    REINSTALL,
    UPGRADE,
    NEW_INSTALL
  };

  // NOTE: If you change this list, you should also change kIconSizes in the cc
  // file.
  enum Icons {
    EXTENSION_ICON_LARGE = 128,
    EXTENSION_ICON_MEDIUM = 48,
    EXTENSION_ICON_SMALL = 32,
    EXTENSION_ICON_SMALLISH = 24,
    EXTENSION_ICON_BITTY = 16,
  };

  // Do not change the order of entries or remove entries in this list
  // as this is used in UMA_HISTOGRAM_ENUMERATIONs about extensions.
  enum Type {
    TYPE_UNKNOWN = 0,
    TYPE_EXTENSION,
    TYPE_THEME,
    TYPE_USER_SCRIPT,
    TYPE_HOSTED_APP,
    TYPE_PACKAGED_APP
  };

  // An NPAPI plugin included in the extension.
  struct PluginInfo {
    FilePath path;  // Path to the plugin.
    bool is_public;  // False if only this extension can load this plugin.
  };

  struct TtsVoice {
    std::string voice_name;
    std::string locale;
    std::string gender;
  };

  // A permission is defined by its |name| (what is used in the manifest),
  // and the |message_id| that's used by install/update UI.
  struct Permission {
    const char* const name;
    const int message_id;
  };

  // |strict_error_checks| enables extra error checking, such as
  // checks that URL patterns do not contain ports.  This error
  // checking may find an error that a previous version of
  // chrome did not flag.  To avoid errors in installed extensions
  // when chrome is upgraded, strict error checking is only enabled
  // when loading extensions as a developer would (such as loading
  // an unpacked extension), or when loading an extension that is
  // tied to a specific version of chrome (such as a component
  // extension).  Most callers will set |strict_error_checks| to
  // Extension::ShouldDoStrictErrorChecking(location).
  static scoped_refptr<Extension> Create(const FilePath& path,
                                         Location location,
                                         const DictionaryValue& value,
                                         bool require_key,
                                         bool strict_error_checks,
                                         std::string* error);

  // Return the update url used by gallery/webstore extensions.
  static GURL GalleryUpdateUrl(bool secure);

  // The install message id for |permission|.  Returns 0 if none exists.
  static int GetPermissionMessageId(const std::string& permission);

  // Returns the full list of permission messages that this extension
  // should display at install time.
  std::vector<string16> GetPermissionMessages() const;

  // Returns the distinct hosts that should be displayed in the install UI
  // for the URL patterns |list|. This discards some of the detail that is
  // present in the manifest to make it as easy as possible to process by
  // users. In particular we disregard the scheme and path components of
  // URLPatterns and de-dupe the result, which includes filtering out common
  // hosts with differing RCDs. (NOTE: when de-duping hosts with common RCDs,
  // the first pattern is returned and the rest discarded)
  static std::vector<std::string> GetDistinctHostsForDisplay(
      const URLPatternList& list);

  // Compares two URLPatternLists for security equality by returning whether
  // the URL patterns in |new_list| contain additional distinct hosts compared
  // to |old_list|.
  static bool IsElevatedHostList(
      const URLPatternList& old_list, const URLPatternList& new_list);

  // Icon sizes used by the extension system.
  static const int kIconSizes[];

  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;
  static const int kSidebarIconMaxSize;

  // Each permission is a module that the extension is permitted to use.
  //
  // NOTE: To add a new permission, define it here, and add an entry to
  // Extension::kPermissions.
  static const char kBackgroundPermission[];
  static const char kBookmarkPermission[];
  static const char kContentSettingsPermission[];
  static const char kContextMenusPermission[];
  static const char kCookiePermission[];
  static const char kExperimentalPermission[];
  static const char kGeolocationPermission[];
  static const char kHistoryPermission[];
  static const char kIdlePermission[];
  static const char kManagementPermission[];
  static const char kNotificationPermission[];
  static const char kProxyPermission[];
  static const char kTabPermission[];
  static const char kUnlimitedStoragePermission[];
  static const char kWebstorePrivatePermission[];

  static const Permission kPermissions[];
  static const size_t kNumPermissions;
  static const char* const kHostedAppPermissionNames[];
  static const size_t kNumHostedAppPermissions;

  // The old name for the unlimited storage permission, which is deprecated but
  // still accepted as meaning the same thing as kUnlimitedStoragePermission.
  static const char kOldUnlimitedStoragePermission[];

  // Valid schemes for web extent URLPatterns.
  static const int kValidWebExtentSchemes;

  // Valid schemes for host permission URLPatterns.
  static const int kValidHostPermissionSchemes;

  // Returns true if the string is one of the known hosted app permissions (see
  // kHostedAppPermissionNames).
  static bool IsHostedAppPermission(const std::string& permission);

  // The name of the manifest inside an extension.
  static const FilePath::CharType kManifestFilename[];

  // The name of locale folder inside an extension.
  static const FilePath::CharType kLocaleFolder[];

  // The name of the messages file inside an extension.
  static const FilePath::CharType kMessagesFilename[];

#if defined(OS_WIN)
  static const char kExtensionRegistryPath[];
#endif

  // The number of bytes in a legal id.
  static const size_t kIdSize;

  // The mimetype used for extensions.
  static const char kMimeType[];

  // Checks to see if the extension has a valid ID.
  static bool IdIsValid(const std::string& id);

  // Generate an ID for an extension in the given path.
  static std::string GenerateIdForPath(const FilePath& file_name);

  // Returns true if the specified file is an extension.
  static bool IsExtension(const FilePath& file_name);

  // Whether the |location| is external or not.
  static inline bool IsExternalLocation(Location location) {
    return location == Extension::EXTERNAL_PREF ||
           location == Extension::EXTERNAL_REGISTRY ||
           location == Extension::EXTERNAL_PREF_DOWNLOAD ||
           location == Extension::EXTERNAL_POLICY_DOWNLOAD;
  }

  // Whether extensions with |location| are auto-updatable or not.
  static inline bool IsAutoUpdateableLocation(Location location) {
    // Only internal and external extensions can be autoupdated.
    return location == Extension::INTERNAL ||
           IsExternalLocation(location);
  }

  // Whether extensions with |location| should be loaded with strict
  // error checking.  Strict error checks may flag errors older versions
  // of chrome did not detect.  To avoid breaking installed extensions,
  // strict checks are disabled unless the location indicates that the
  // developer is loading the extension, or the extension is a component
  // of chrome.
  static inline bool ShouldDoStrictErrorChecking(Location location) {
    return location == Extension::LOAD ||
           location == Extension::COMPONENT;
  }

  // See Type definition above.
  Type GetType() const;

  // Returns an absolute url to a resource inside of an extension. The
  // |extension_url| argument should be the url() from an Extension object. The
  // |relative_path| can be untrusted user input. The returned URL will either
  // be invalid() or a child of |extension_url|.
  // NOTE: Static so that it can be used from multiple threads.
  static GURL GetResourceURL(const GURL& extension_url,
                             const std::string& relative_path);
  GURL GetResourceURL(const std::string& relative_path) const {
    return GetResourceURL(url(), relative_path);
  }

  // Returns an extension resource object. |relative_path| should be UTF8
  // encoded.
  ExtensionResource GetResource(const std::string& relative_path) const;

  // As above, but with |relative_path| following the file system's encoding.
  ExtensionResource GetResource(const FilePath& relative_path) const;

  // |input| is expected to be the text of an rsa public or private key. It
  // tolerates the presence or absence of bracking header/footer like this:
  //     -----(BEGIN|END) [RSA PUBLIC/PRIVATE] KEY-----
  // and may contain newlines.
  static bool ParsePEMKeyBytes(const std::string& input, std::string* output);

  // Does a simple base64 encoding of |input| into |output|.
  static bool ProducePEM(const std::string& input, std::string* output);

  // Generates an extension ID from arbitrary input. The same input string will
  // always generate the same output ID.
  static bool GenerateId(const std::string& input, std::string* output);

  // Expects base64 encoded |input| and formats into |output| including
  // the appropriate header & footer.
  static bool FormatPEMForFileOutput(const std::string input,
      std::string* output, bool is_public);

  // Determine whether |new_extension| has increased privileges compared to
  // its previously granted permissions, specified by |granted_apis|,
  // |granted_extent| and |granted_full_access|.
  static bool IsPrivilegeIncrease(const bool granted_full_access,
                                  const std::set<std::string>& granted_apis,
                                  const ExtensionExtent& granted_extent,
                                  const Extension* new_extension);

  // Given an extension and icon size, read it if present and decode it into
  // result. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoadingTracker.
  static void DecodeIcon(const Extension* extension,
                         Icons icon_size,
                         scoped_ptr<SkBitmap>* result);

  // Given an icon_path and icon size, read it if present and decode it into
  // result. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoadingTracker.
  static void DecodeIconFromPath(const FilePath& icon_path,
                                 Icons icon_size,
                                 scoped_ptr<SkBitmap>* result);

  // Returns the base extension url for a given |extension_id|.
  static GURL GetBaseURLFromExtensionId(const std::string& extension_id);

  // Returns the url prefix for the extension/apps gallery. Can be set via the
  // --apps-gallery-url switch. The URL returned will not contain a trailing
  // slash. Do not use this as a prefix/extent for the store.  Instead see
  // ExtensionService::GetWebStoreApp or
  // ExtensionService::IsDownloadFromGallery
  static std::string ChromeStoreLaunchURL();

  // Adds an extension to the scripting whitelist. Used for testing only.
  static void SetScriptingWhitelist(const ScriptingWhitelist& whitelist);
  static const ScriptingWhitelist* GetScriptingWhitelist();

  // Returns true if the extension has the specified API permission.
  static bool HasApiPermission(const std::set<std::string>& api_permissions,
                               const std::string& function_name);

  // Whether the |effective_host_permissions| and |api_permissions| include
  // effective access to all hosts. See the non-static version of the method
  // for more details.
  static bool HasEffectiveAccessToAllHosts(
      const ExtensionExtent& effective_host_permissions,
      const std::set<std::string>& api_permissions);

  bool HasApiPermission(const std::string& function_name) const {
    return HasApiPermission(this->api_permissions(), function_name);
  }

  const ExtensionExtent& GetEffectiveHostPermissions() const {
    return effective_host_permissions_;
  }

  // Whether or not the extension is allowed permission for a URL pattern from
  // the manifest.  http, https, and chrome://favicon/ is allowed for all
  // extensions, while component extensions are allowed access to
  // chrome://resources.
  bool CanSpecifyHostPermission(const URLPattern& pattern) const;

  // Whether the extension has access to the given URL.
  bool HasHostPermission(const GURL& url) const;

  // Whether the extension has effective access to all hosts. This is true if
  // there is a content script that matches all hosts, if there is a host
  // permission grants access to all hosts (like <all_urls>) or an api
  // permission that effectively grants access to all hosts (e.g. proxy,
  // network, etc.)
  bool HasEffectiveAccessToAllHosts() const;

  // Whether the extension effectively has all permissions (for example, by
  // having an NPAPI plugin).
  bool HasFullPermissions() const;

  // Whether context menu should be shown for page and browser actions.
  bool ShowConfigureContextMenus() const;

  // Returns the Homepage URL for this extension. If homepage_url was not
  // specified in the manifest, this returns the Google Gallery URL. For
  // third-party extensions, this returns a blank GURL.
  GURL GetHomepageURL() const;

  // Returns a list of paths (relative to the extension dir) for images that
  // the browser might load (like themes and page action icons).
  std::set<FilePath> GetBrowserImages() const;

  // Get an extension icon as a resource or URL.
  ExtensionResource GetIconResource(
      int size, ExtensionIconSet::MatchType match_type) const;
  GURL GetIconURL(int size, ExtensionIconSet::MatchType match_type) const;

  // Gets the fully resolved absolute launch URL.
  GURL GetFullLaunchURL() const;

  // Image cache related methods. These are only valid on the UI thread and
  // not maintained by this class. See ImageLoadingTracker for usage. The
  // |original_size| parameter should be the size of the image at |source|
  // before any scaling may have been done to produce the pixels in |image|.
  void SetCachedImage(const ExtensionResource& source,
                      const SkBitmap& image,
                      const gfx::Size& original_size) const;
  bool HasCachedImage(const ExtensionResource& source,
                      const gfx::Size& max_size) const;
  SkBitmap GetCachedImage(const ExtensionResource& source,
                          const gfx::Size& max_size) const;

  // Returns true if this extension can execute script on a page. If a
  // UserScript object is passed, permission to run that specific script is
  // checked (using its matches list). Otherwise, permission to execute script
  // programmatically is checked (using the extension's host permission).
  //
  // This method is also aware of certain special pages that extensions are
  // usually not allowed to run script on.
  bool CanExecuteScriptOnPage(const GURL& page_url,
                              UserScript* script,
                              std::string* error) const;

  // Returns true if this extension is a COMPONENT extension, or if it is
  // on the whitelist of extensions that can script all pages.
  bool CanExecuteScriptEverywhere() const;

  // Returns true if this extension is allowed to obtain the contents of a
  // page as an image.  Since a page may contain sensitive information, this
  // is restricted to the extension's host permissions as well as the
  // extension page itself.
  bool CanCaptureVisiblePage(const GURL& page_url, std::string* error) const;

  // Returns true if this extension updates itself using the extension
  // gallery.
  bool UpdatesFromGallery() const;

  // Returns true if this extension or app includes areas within |origin|.
  bool OverlapsWithOrigin(const GURL& origin) const;

  // Accessors:

  const FilePath& path() const { return path_; }
  const GURL& url() const { return extension_url_; }
  Location location() const { return location_; }
  const std::string& id() const { return id_; }
  const Version* version() const { return version_.get(); }
  const std::string VersionString() const;
  const std::string& name() const { return name_; }
  const std::string& public_key() const { return public_key_; }
  const std::string& description() const { return description_; }
  bool converted_from_user_script() const {
    return converted_from_user_script_;
  }
  const UserScriptList& content_scripts() const { return content_scripts_; }
  ExtensionAction* page_action() const { return page_action_.get(); }
  ExtensionAction* browser_action() const { return browser_action_.get(); }
  ExtensionSidebarDefaults* sidebar_defaults() const {
    return sidebar_defaults_.get();
  }
  const std::vector<PluginInfo>& plugins() const { return plugins_; }
  const GURL& background_url() const { return background_url_; }
  const GURL& options_url() const { return options_url_; }
  const GURL& devtools_url() const { return devtools_url_; }
  const std::vector<GURL>& toolstrips() const { return toolstrips_; }
  const std::set<std::string>& api_permissions() const {
    return api_permissions_;
  }
  const URLPatternList& host_permissions() const { return host_permissions_; }
  const GURL& update_url() const { return update_url_; }
  const ExtensionIconSet& icons() const { return icons_; }
  const DictionaryValue* manifest_value() const {
    return manifest_value_.get();
  }
  const std::string default_locale() const { return default_locale_; }
  const URLOverrideMap& GetChromeURLOverrides() const {
    return chrome_url_overrides_;
  }
  const std::string omnibox_keyword() const { return omnibox_keyword_; }
  bool incognito_split_mode() const { return incognito_split_mode_; }
  const std::vector<TtsVoice>& tts_voices() const { return tts_voices_; }

  // App-related.
  bool is_app() const { return is_app_; }
  bool is_hosted_app() const { return is_app() && !web_extent().is_empty(); }
  bool is_packaged_app() const { return is_app() && web_extent().is_empty(); }
  const ExtensionExtent& web_extent() const { return extent_; }
  const std::string& launch_local_path() const { return launch_local_path_; }
  const std::string& launch_web_url() const { return launch_web_url_; }
  extension_misc::LaunchContainer launch_container() const {
    return launch_container_;
  }
  int launch_width() const { return launch_width_; }
  int launch_height() const { return launch_height_; }

  // Theme-related.
  bool is_theme() const { return is_theme_; }
  DictionaryValue* GetThemeImages() const { return theme_images_.get(); }
  DictionaryValue* GetThemeColors() const {return theme_colors_.get(); }
  DictionaryValue* GetThemeTints() const { return theme_tints_.get(); }
  DictionaryValue* GetThemeDisplayProperties() const {
    return theme_display_properties_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<Extension>;

  // We keep a cache of images loaded from extension resources based on their
  // path and a string representation of a size that may have been used to
  // scale it (or the empty string if the image is at its original size).
  typedef std::pair<FilePath, std::string> ImageCacheKey;
  typedef std::map<ImageCacheKey, SkBitmap> ImageCache;

  // Normalize the path for use by the extension. On Windows, this will make
  // sure the drive letter is uppercase.
  static FilePath MaybeNormalizePath(const FilePath& path);

  // Returns the distinct hosts that can be displayed in the install UI or be
  // used for privilege comparisons. This discards some of the detail that is
  // present in the manifest to make it as easy as possible to process by users.
  // In particular we disregard the scheme and path components of URLPatterns
  // and de-dupe the result, which includes filtering out common hosts with
  // differing RCDs. If |include_rcd| is true, then the de-duped result
  // will be the first full entry, including its RCD. So if the list was
  // "*.google.co.uk" and "*.google.com", the returned value would just be
  // "*.google.co.uk". Keeping the RCD in the result is useful for display
  // purposes when you want to show the user one sample hostname from the list.
  // If you need to compare two URLPatternLists for security equality, then set
  // |include_rcd| to false, which will return a result like "*.google.",
  // regardless of the order of the patterns.
  static std::vector<std::string> GetDistinctHosts(
      const URLPatternList& host_patterns, bool include_rcd);

  Extension(const FilePath& path, Location location);
  ~Extension();

  // Initialize the extension from a parsed manifest.
  // Usually, the id of an extension is generated by the "key" property of
  // its manifest, but if |require_key| is |false|, a temporary ID will be
  // generated based on the path.
  bool InitFromValue(const DictionaryValue& value, bool require_key,
                     bool strict_error_checks, std::string* error);

  // Helper function for implementing HasCachedImage/GetCachedImage. A return
  // value of NULL means there is no matching image cached (we allow caching an
  // empty SkBitmap).
  SkBitmap* GetCachedImageImpl(const ExtensionResource& source,
                               const gfx::Size& max_size) const;

  // Helper method that loads a UserScript object from a
  // dictionary in the content_script list of the manifest.
  bool LoadUserScriptHelper(const DictionaryValue* content_script,
                            int definition_index,
                            URLPattern::ParseOption parse_strictness,
                            std::string* error,
                            UserScript* result);

  // Helper method that loads either the include_globs or exclude_globs list
  // from an entry in the content_script lists of the manifest.
  bool LoadGlobsHelper(const DictionaryValue* content_script,
                       int content_script_index,
                       const char* globs_property_name,
                       std::string* error,
                       void(UserScript::*add_method)(const std::string& glob),
                       UserScript *instance);

  // Helpers to load various chunks of the manifest.
  bool LoadIsApp(const DictionaryValue* manifest, std::string* error);
  bool LoadExtent(const DictionaryValue* manifest,
                  const char* key,
                  ExtensionExtent* extent,
                  const char* list_error,
                  const char* value_error,
                  URLPattern::ParseOption parse_strictness,
                  std::string* error);
  bool LoadLaunchContainer(const DictionaryValue* manifest, std::string* error);
  bool LoadLaunchURL(const DictionaryValue* manifest, std::string* error);
  bool EnsureNotHybridApp(const DictionaryValue* manifest, std::string* error);

  // Helper method to load an ExtensionAction from the page_action or
  // browser_action entries in the manifest.
  ExtensionAction* LoadExtensionActionHelper(
      const DictionaryValue* extension_action, std::string* error);

  // Helper method to load an ExtensionSidebarDefaults from the sidebar manifest
  // entry.
  ExtensionSidebarDefaults* LoadExtensionSidebarDefaults(
      const DictionaryValue* sidebar, std::string* error);

  // Calculates the effective host permissions from the permissions and content
  // script petterns.
  void InitEffectiveHostPermissions();

  // Returns true if the extension has more than one "UI surface". For example,
  // an extension that has a browser action and a page action.
  bool HasMultipleUISurfaces() const;

  // Figures out if a source contains keys not associated with themes - we
  // don't want to allow scripts and such to be bundled with themes.
  bool ContainsNonThemeKeys(const DictionaryValue& source) const;

  // Returns true if the string is one of the known api permissions (see
  // kPermissions).
  bool IsAPIPermission(const std::string& permission) const;

  // The set of unique API install messages that the extension has.
  // NOTE: This only includes messages related to permissions declared in the
  // "permissions" key in the manifest.  Permissions implied from other features
  // of the manifest, like plugins and content scripts are not included.
  std::set<string16> GetSimplePermissionMessages() const;

  // The permission message displayed related to the host permissions for
  // this extension.
  string16 GetHostPermissionMessage() const;

  // Cached images for this extension. This should only be touched on the UI
  // thread.
  mutable ImageCache image_cache_;

  // A persistent, globally unique ID. An extension's ID is used in things
  // like directory structures and URLs, and is expected to not change across
  // versions. It is generated as a SHA-256 hash of the extension's public
  // key, or as a hash of the path in the case of unpacked extensions.
  std::string id_;

  // The extension's human-readable name. Name is used for display purpose. It
  // might be wrapped with unicode bidi control characters so that it is
  // displayed correctly in RTL context.
  // NOTE: Name is UTF-8 and may contain non-ascii characters.
  std::string name_;

  // The absolute path to the directory the extension is stored in.
  FilePath path_;

  // Default locale for fall back. Can be empty if extension is not localized.
  std::string default_locale_;

  // If true, a separate process will be used for the extension in incognito
  // mode.
  bool incognito_split_mode_;

  // Defines the set of URLs in the extension's web content.
  ExtensionExtent extent_;

  // The set of host permissions that the extension effectively has access to,
  // which is a merge of host_permissions_ and all of the match patterns in
  // any content scripts the extension has. This is used to determine which
  // URLs have the ability to load an extension's resources via embedded
  // chrome-extension: URLs (see extension_protocols.cc).
  ExtensionExtent effective_host_permissions_;

  // The set of module-level APIs this extension can use.
  std::set<std::string> api_permissions_;

  // The icons for the extension.
  ExtensionIconSet icons_;

  // The base extension url for the extension.
  GURL extension_url_;

  // The location the extension was loaded from.
  Location location_;

  // The extension's version.
  scoped_ptr<Version> version_;

  // An optional longer description of the extension.
  std::string description_;

  // True if the extension was generated from a user script. (We show slightly
  // different UI if so).
  bool converted_from_user_script_;

  // Paths to the content scripts the extension contains.
  UserScriptList content_scripts_;

  // The extension's page action, if any.
  scoped_ptr<ExtensionAction> page_action_;

  // The extension's browser action, if any.
  scoped_ptr<ExtensionAction> browser_action_;

  // The extension's sidebar, if any.
  scoped_ptr<ExtensionSidebarDefaults> sidebar_defaults_;

  // Optional list of NPAPI plugins and associated properties.
  std::vector<PluginInfo> plugins_;

  // Optional URL to a master page of which a single instance should be always
  // loaded in the background.
  GURL background_url_;

  // Optional URL to a page for setting options/preferences.
  GURL options_url_;

  // Optional URL to a devtools extension page.
  GURL devtools_url_;

  // Optional list of toolstrips and associated properties.
  std::vector<GURL> toolstrips_;

  // The public key used to sign the contents of the crx package.
  std::string public_key_;

  // A map of resource id's to relative file paths.
  scoped_ptr<DictionaryValue> theme_images_;

  // A map of color names to colors.
  scoped_ptr<DictionaryValue> theme_colors_;

  // A map of color names to colors.
  scoped_ptr<DictionaryValue> theme_tints_;

  // A map of display properties.
  scoped_ptr<DictionaryValue> theme_display_properties_;

  // Whether the extension is a theme.
  bool is_theme_;

  // The sites this extension has permission to talk to (using XHR, etc).
  URLPatternList host_permissions_;

  // The homepage for this extension. Useful if it is not hosted by Google and
  // therefore does not have a Gallery URL.
  GURL homepage_url_;

  // URL for fetching an update manifest
  GURL update_url_;

  // A copy of the manifest that this extension was created from.
  scoped_ptr<DictionaryValue> manifest_value_;

  // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
  // which override the handling of those URLs. (see ExtensionOverrideUI).
  URLOverrideMap chrome_url_overrides_;

  // Whether this extension uses app features.
  bool is_app_;

  // The local path inside the extension to use with the launcher.
  std::string launch_local_path_;

  // A web url to use with the launcher. Note that this might be relative or
  // absolute. If relative, it is relative to web_origin.
  std::string launch_web_url_;

  // The window type that an app's manifest specifies to launch into.
  // This is not always the window type an app will open into, because
  // users can override the way each app launches.  See
  // ExtensionPrefs::GetLaunchContainer(), which looks at a per-app pref
  // to decide what container an app will launch in.
  extension_misc::LaunchContainer launch_container_;

  // The default size of the container when launching. Only respected for
  // containers like panels and windows.
  int launch_width_;
  int launch_height_;

  // The Omnibox keyword for this extension, or empty if there is none.
  std::string omnibox_keyword_;

  // List of text-to-speech voices that this extension provides, if any.
  std::vector<TtsVoice> tts_voices_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           UpdateExtensionPreservesLocation);
  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, LoadPageActionHelper);
  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, InitFromValueInvalid);
  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, InitFromValueValid);
  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, InitFromValueValidNameInRTL);
  FRIEND_TEST_ALL_PREFIXES(TabStripModelTest, Apps);

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

typedef std::vector< scoped_refptr<const Extension> > ExtensionList;
typedef std::set<std::string> ExtensionIdSet;

// Handy struct to pass core extension info around.
struct ExtensionInfo {
  ExtensionInfo(const DictionaryValue* manifest,
                const std::string& id,
                const FilePath& path,
                Extension::Location location);
  ~ExtensionInfo();

  scoped_ptr<DictionaryValue> extension_manifest;
  std::string extension_id;
  FilePath extension_path;
  Extension::Location extension_location;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInfo);
};

// Struct used for the details of the EXTENSION_UNINSTALLED
// notification.
struct UninstalledExtensionInfo {
  explicit UninstalledExtensionInfo(const Extension& extension);
  ~UninstalledExtensionInfo();

  std::string extension_id;
  std::set<std::string> extension_api_permissions;
  Extension::Type extension_type;
  GURL update_url;
};

struct UnloadedExtensionInfo {
  enum Reason {
    DISABLE,    // The extension is being disabled.
    UPDATE,     // The extension is being updated to a newer version.
    UNINSTALL,  // The extension is being uninstalled.
  };

  Reason reason;

  // Was the extension already disabled?
  bool already_disabled;

  // The extension being unloaded - this should always be non-NULL.
  const Extension* extension;

  UnloadedExtensionInfo(const Extension* extension, Reason reason);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
