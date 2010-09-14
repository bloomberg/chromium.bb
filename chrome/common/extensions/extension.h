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
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/extensions/url_pattern.h"
#include "gfx/size.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class ExtensionAction;
class ExtensionResource;
class SkBitmap;
class Version;

// Represents a Chrome extension.
class Extension {
 public:
  typedef std::vector<URLPattern> URLPatternList;
  typedef std::map<const std::string, GURL> URLOverrideMap;

  // What an extension was loaded from.
  // NOTE: These values are stored as integers in the preferences, so you
  // really don't want to change any existing ones.
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
    EXTERNAL_PREF_DOWNLOAD  // A crx file from an external directory (via
                            // prefs), installed from an update URL.
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

  enum LaunchContainer {
    LAUNCH_WINDOW,
    LAUNCH_PANEL,
    LAUNCH_TAB
  };

  bool apps_enabled() const { return apps_enabled_; }
  void set_apps_enabled(bool val) { apps_enabled_ = val; }

  // Icon sizes used by the extension system.
  static const int kIconSizes[];

  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;

  // Each permission is a module that the extension is permitted to use.
  //
  // NOTE: If you add a permission, consider also changing:
  // - Extension::GetSimplePermissions()
  // - Extension::IsPrivilegeIncrease()
  // - ExtensionInstallUI::GetV2Warnings()
  static const char kBackgroundPermission[];
  static const char kBookmarkPermission[];
  static const char kContextMenusPermission[];
  static const char kCookiePermission[];
  static const char kExperimentalPermission[];
  static const char kGeolocationPermission[];
  static const char kHistoryPermission[];
  static const char kIdlePermission[];
  static const char kNotificationPermission[];
  static const char kProxyPermission[];
  static const char kTabPermission[];
  static const char kUnlimitedStoragePermission[];
  static const char kWebstorePrivatePermission[];

  static const char* const kPermissionNames[];
  static const size_t kNumPermissions;
  static const char* const kHostedAppPermissionNames[];
  static const size_t kNumHostedAppPermissions;

  // The old name for the unlimited storage permission, which is deprecated but
  // still accepted as meaning the same thing as kUnlimitedStoragePermission.
  static const char kOldUnlimitedStoragePermission[];

  // A "simple permission" is one that has a one-to-one mapping with a message
  // that is displayed in the install UI. This is in contrast to more complex
  // permissions like http access, where the exact message displayed depends on
  // several factors.
  typedef std::map<std::string, string16> SimplePermissions;
  static const SimplePermissions& GetSimplePermissions();

  // Returns true if the string is one of the known hosted app permissions (see
  // kHostedAppPermissionNames).
  static bool IsHostedAppPermission(const std::string& permission);

  // An NPAPI plugin included in the extension.
  struct PluginInfo {
    FilePath path;  // Path to the plugin.
    bool is_public;  // False if only this extension can load this plugin.
  };

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

  explicit Extension(const FilePath& path);
  virtual ~Extension();

  // Checks to see if the extension has a valid ID.
  static bool IdIsValid(const std::string& id);

  // Returns true if the specified file is an extension.
  static bool IsExtension(const FilePath& file_name);

  // Whether the |location| is external or not.
  static inline bool IsExternalLocation(Location location) {
    return location == Extension::EXTERNAL_PREF ||
           location == Extension::EXTERNAL_REGISTRY ||
           location == Extension::EXTERNAL_PREF_DOWNLOAD;
  }

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
  ExtensionResource GetResource(const std::string& relative_path);

  // As above, but with |relative_path| following the file system's encoding.
  ExtensionResource GetResource(const FilePath& relative_path);

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
  // |old_extension|.
  static bool IsPrivilegeIncrease(Extension* old_extension,
                                  Extension* new_extension);

  // Given an extension and icon size, read it if present and decode it into
  // result. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoadingTracker.
  static void DecodeIcon(Extension* extension,
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

  // Returns whether the browser has apps enabled (either as the default or if
  // it was explicitly turned on via a command line switch).
  static bool AppsAreEnabled();

  // Returns the url prefix for the extension/apps gallery. Can be set via the
  // --apps-gallery-url switch. The URL returned will not contain a trailing
  // slash.
  static std::string ChromeStoreURL();

  // Initialize the extension from a parsed manifest.
  // Usually, the id of an extension is generated by the "key" property of
  // its manifest, but if |require_key| is |false|, a temporary ID will be
  // generated based on the path.
  bool InitFromValue(const DictionaryValue& value, bool require_key,
                     std::string* error);

  const FilePath& path() const { return path_; }
  void set_path(const FilePath& path) { path_ = path; }
  const GURL& url() const { return extension_url_; }
  Location location() const { return location_; }
  void set_location(Location location) { location_ = location; }
  const std::string& id() const { return id_; }
  const Version* version() const { return version_.get(); }
  // String representation of the version number.
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
  const std::vector<PluginInfo>& plugins() const { return plugins_; }
  const GURL& background_url() const { return background_url_; }
  const GURL& options_url() const { return options_url_; }
  const GURL& devtools_url() const { return devtools_url_; }
  const std::vector<GURL>& toolstrips() const { return toolstrips_; }
  const std::vector<std::string>& api_permissions() const {
    return api_permissions_;
  }
  const URLPatternList& host_permissions() const {
    return host_permissions_;
  }

  // Returns true if the extension has the specified API permission.
  static bool HasApiPermission(const std::vector<std::string>& api_permissions,
                               const std::string& function_name);

  bool HasApiPermission(const std::string& function_name) const {
    return HasApiPermission(this->api_permissions(), function_name);
  }

  // Returns the set of hosts that the extension effectively has access to. This
  // is used in the permissions UI and is a combination of the hosts accessible
  // through content scripts and the hosts accessible through XHR.
  const ExtensionExtent GetEffectiveHostPermissions() const;

  // Whether or not the extension is allowed permission for a URL pattern from
  // the manifest.  http, https, and chrome://favicon/ is allowed for all
  // extensions, while component extensions are allowed access to
  // chrome://resources.
  bool CanAccessURL(const URLPattern pattern) const;

  // Whether the extension has access to the given URL.
  bool HasHostPermission(const GURL& url) const;

  // Returns true if the extension effectively has access to the user's browsing
  // history.  There are several permissions that we group together into this
  // bucket.  For example: tabs, bookmarks, and history.
  bool HasEffectiveBrowsingHistoryPermission() const;

  // Whether the extension has access to all hosts. This is true if there is
  // a content script that matches all hosts, or if there is a host permission
  // for all hosts.
  bool HasAccessToAllHosts() const;

  const GURL& update_url() const { return update_url_; }
  const std::map<int, std::string>& icons() const { return icons_; }

  // Returns the Google Gallery URL for this extension, if one exists. For
  // third-party extensions, this returns a blank GURL.
  GURL GalleryUrl() const;

  // Theme-related.
  DictionaryValue* GetThemeImages() const { return theme_images_.get(); }
  DictionaryValue* GetThemeColors() const { return theme_colors_.get(); }
  DictionaryValue* GetThemeTints() const { return theme_tints_.get(); }
  DictionaryValue* GetThemeDisplayProperties() const {
    return theme_display_properties_.get();
  }
  bool is_theme() const { return is_theme_; }

  // Returns a list of paths (relative to the extension dir) for images that
  // the browser might load (like themes and page action icons).
  std::set<FilePath> GetBrowserImages();

  // Returns an absolute path to the given icon inside of the extension. Returns
  // an empty FilePath if the extension does not have that icon.
  ExtensionResource GetIconResource(Icons icon);

  // Looks for an extension icon of dimension |icon|. If not found, checks if
  // the next larger size exists (until one is found or the end is reached). If
  // an icon is found, the path is returned in |resource| and the dimension
  // found is returned to the caller (as function return value).
  // NOTE: |resource| is not guaranteed to be non-empty.
  Icons GetIconResourceAllowLargerSize(ExtensionResource* resource, Icons icon);

  GURL GetIconURL(Icons icon);
  GURL GetIconURLAllowLargerSize(Icons icon);

  const DictionaryValue* manifest_value() const {
    return manifest_value_.get();
  }

  const std::string default_locale() const { return default_locale_; }

  // Chrome URL overrides (see ExtensionOverrideUI).
  const URLOverrideMap& GetChromeURLOverrides() const {
    return chrome_url_overrides_;
  }

  const std::string omnibox_keyword() const { return omnibox_keyword_; }

  bool is_app() const { return is_app_; }
  ExtensionExtent& web_extent() { return web_extent_; }
  const std::string& launch_local_path() const { return launch_local_path_; }
  const std::string& launch_web_url() const { return launch_web_url_; }
  void set_launch_web_url(const std::string& launch_web_url) {
    launch_web_url_ = launch_web_url;
  }
  LaunchContainer launch_container() const { return launch_container_; }
  int launch_width() const { return launch_width_; }
  int launch_height() const { return launch_height_; }
  bool incognito_split_mode() const { return incognito_split_mode_; }

  // Gets the fully resolved absolute launch URL.
  GURL GetFullLaunchURL() const;

  // Runtime data:
  // Put dynamic data about the state of a running extension below.

  // Whether the background page, if any, is ready. We don't load other
  // components until then. If there is no background page, we consider it to
  // be ready.
  bool GetBackgroundPageReady();
  void SetBackgroundPageReady();

  // Getter and setter for the flag that specifies whether the extension is
  // being upgraded.
  bool being_upgraded() const { return being_upgraded_; }
  void set_being_upgraded(bool value) { being_upgraded_ = value; }

  // Image cache related methods. These are only valid on the UI thread and
  // not maintained by this class. See ImageLoadingTracker for usage. The
  // |original_size| parameter should be the size of the image at |source|
  // before any scaling may have been done to produce the pixels in |image|.
  void SetCachedImage(const ExtensionResource& source,
                      const SkBitmap& image,
                      const gfx::Size& original_size);
  bool HasCachedImage(const ExtensionResource& source,
                      const gfx::Size& max_size);
  SkBitmap GetCachedImage(const ExtensionResource& source,
                          const gfx::Size& max_size);
  bool is_hosted_app() { return is_app() && !web_extent().is_empty(); }
  bool is_packaged_app() { return is_app() && web_extent().is_empty(); }
 private:
  // We keep a cache of images loaded from extension resources based on their
  // path and a string representation of a size that may have been used to
  // scale it (or the empty string if the image is at its original size).
  typedef std::pair<FilePath, std::string> ImageCacheKey;
  typedef std::map<ImageCacheKey, SkBitmap> ImageCache;

  // Helper function for implementing HasCachedImage/GetCachedImage. A return
  // value of NULL means there is no matching image cached (we allow caching an
  // empty SkBitmap).
  SkBitmap* GetCachedImageImpl(const ExtensionResource& source,
                               const gfx::Size& max_size);

  // Helper method that loads a UserScript object from a
  // dictionary in the content_script list of the manifest.
  bool LoadUserScriptHelper(const DictionaryValue* content_script,
                            int definition_index,
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
  bool LoadExtent(const DictionaryValue* manifest, const char* key,
                  ExtensionExtent* extent, const char* list_error,
                  const char* value_error, std::string* error);
  bool LoadLaunchContainer(const DictionaryValue* manifest, std::string* error);
  bool LoadLaunchURL(const DictionaryValue* manifest, std::string* error);
  bool EnsureNotHybridApp(const DictionaryValue* manifest, std::string* error);

  // Helper method to load an ExtensionAction from the page_action or
  // browser_action entries in the manifest.
  ExtensionAction* LoadExtensionActionHelper(
      const DictionaryValue* extension_action, std::string* error);

  // Figures out if a source contains keys not associated with themes - we
  // don't want to allow scripts and such to be bundled with themes.
  bool ContainsNonThemeKeys(const DictionaryValue& source);

  // Returns true if the string is one of the known api permissions (see
  // kPermissionNames).
  bool IsAPIPermission(const std::string& permission);

  // Utility functions to get the icon relative path used to create an
  // ExtensionResource or URL.
  std::string GetIconPath(Icons icon);
  Icons GetIconPathAllowLargerSize(std::string* path, Icons icon);

  // The absolute path to the directory the extension is stored in.
  FilePath path_;

  // The base extension url for the extension.
  GURL extension_url_;

  // The location the extension was loaded from.
  Location location_;

  // A human-readable ID for the extension. The convention is to use something
  // like 'com.example.myextension', but this is not currently enforced. An
  // extension's ID is used in things like directory structures and URLs, and
  // is expected to not change across versions. In the case of conflicts,
  // updates will only be allowed if the extension can be validated using the
  // previous version's update key.
  std::string id_;

  // The extension's version.
  scoped_ptr<Version> version_;

  // The extension's human-readable name. Name is used for display purpose. It
  // might be wrapped with unicode bidi control characters so that it is
  // displayed correctly in RTL context.
  // NOTE: Name is UTF-8 and may contain non-ascii characters.
  std::string name_;

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

  // Optional list of NPAPI plugins and associated properties.
  std::vector<PluginInfo> plugins_;

  // Optional URL to a master page of which a single instance should be always
  // loaded in the background.
  GURL background_url_;

  // Optional URL to a page for setting options/preferences.
  GURL options_url_;

  // Optional URL to a devtools extension page.
  GURL devtools_url_;

  // Optional list of toolstrips_ and associated properties.
  std::vector<GURL> toolstrips_;

  // The public key ('key' in the manifest) used to sign the contents of the
  // crx package ('signature' in the manifest)
  std::string public_key_;

  // A map of resource id's to relative file paths.
  scoped_ptr<DictionaryValue> theme_images_;

  // A map of color names to colors.
  scoped_ptr<DictionaryValue> theme_colors_;

  // A map of color names to colors.
  scoped_ptr<DictionaryValue> theme_tints_;

  // A map of display properties.
  scoped_ptr<DictionaryValue> theme_display_properties_;

  // Whether the extension is a theme - if it is, certain things are disabled.
  bool is_theme_;

  // The set of module-level APIs this extension can use.
  std::vector<std::string> api_permissions_;

  // The sites this extension has permission to talk to (using XHR, etc).
  URLPatternList host_permissions_;

  // The paths to the icons the extension contains mapped by their width.
  std::map<int, std::string> icons_;

  // URL for fetching an update manifest
  GURL update_url_;

  // A copy of the manifest that this extension was created from.
  scoped_ptr<DictionaryValue> manifest_value_;

  // Default locale for fall back. Can be empty if extension is not localized.
  std::string default_locale_;

  // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
  // which override the handling of those URLs.
  URLOverrideMap chrome_url_overrides_;

  // Whether apps-related features can be parsed during InitFromValue().
  // Defaults to the value from --enable-extension-apps.
  bool apps_enabled_;

  // Whether this extension uses app features.
  bool is_app_;

  // Defines the set of URLs in the extension's web content.
  ExtensionExtent web_extent_;

  // The local path inside the extension to use with the launcher.
  std::string launch_local_path_;

  // A web url to use with the launcher. Note that this might be relative or
  // absolute. If relative, it is relative to web_origin_.
  std::string launch_web_url_;

  // The type of container to launch into.
  LaunchContainer launch_container_;

  // The default size of the container when launching. Only respected for
  // containers like panels and windows.
  int launch_width_;
  int launch_height_;

  // Cached images for this extension.
  ImageCache image_cache_;

  // The omnibox keyword for this extension, or empty if there is none.
  std::string omnibox_keyword_;

  // If true, a separate process will be used for the extension in incognito
  // mode.
  bool incognito_split_mode_;

  // Runtime data:

  // True if the background page is ready.
  bool background_page_ready_;

  // True while the extension is being upgraded.
  bool being_upgraded_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, LoadPageActionHelper);
  FRIEND_TEST_ALL_PREFIXES(TabStripModelTest, Apps);

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

typedef std::vector<Extension*> ExtensionList;

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

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
