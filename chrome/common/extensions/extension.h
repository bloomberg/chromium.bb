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

  // Type used for UMA_HISTOGRAM_ENUMERATION about extensions.
  // Do not change the order of entries or remove entries in this list.
  enum HistogramType {
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

  // Contains a subset of the extension's data that doesn't change once
  // initialized, and therefore shareable across threads without locking.
  struct StaticData : public base::RefCountedThreadSafe<StaticData> {
    StaticData();

    // TODO(mpcomplete): RefCountedThreadSafe does not allow AddRef/Release on
    // const objects. I think that is a mistake. Until we can fix that, here's
    // a workaround.
    void AddRef() const {
      const_cast<StaticData*>(this)->
          base::RefCountedThreadSafe<StaticData>::AddRef();
    }
    void Release() const {
      const_cast<StaticData*>(this)->
          base::RefCountedThreadSafe<StaticData>::Release();
    }

    // A persistent, globally unique ID. An extension's ID is used in things
    // like directory structures and URLs, and is expected to not change across
    // versions. It is generated as a SHA-256 hash of the extension's public
    // key, or as a hash of the path in the case of unpacked extensions.
    std::string id;

    // The extension's human-readable name. Name is used for display purpose. It
    // might be wrapped with unicode bidi control characters so that it is
    // displayed correctly in RTL context.
    // NOTE: Name is UTF-8 and may contain non-ascii characters.
    std::string name;

    // The absolute path to the directory the extension is stored in.
    FilePath path;

    // Default locale for fall back. Can be empty if extension is not localized.
    std::string default_locale;

    // If true, a separate process will be used for the extension in incognito
    // mode.
    bool incognito_split_mode;

    // Defines the set of URLs in the extension's web content.
    ExtensionExtent extent;

    // The set of host permissions that the extension effectively has access to,
    // which is a merge of host_permissions_ and all of the match patterns in
    // any content scripts the extension has. This is used to determine which
    // URLs have the ability to load an extension's resources via embedded
    // chrome-extension: URLs (see extension_protocols.cc).
    ExtensionExtent effective_host_permissions;

    // The set of module-level APIs this extension can use.
    std::set<std::string> api_permissions;

    // The icons for the extension.
    ExtensionIconSet icons;

    // The base extension url for the extension.
    GURL extension_url;

    // The location the extension was loaded from.
    Location location;

    // The extension's version.
    scoped_ptr<Version> version;

    // An optional longer description of the extension.
    std::string description;

    // True if the extension was generated from a user script. (We show slightly
    // different UI if so).
    bool converted_from_user_script;

    // Paths to the content scripts the extension contains.
    UserScriptList content_scripts;

    // The extension's page action, if any.
    scoped_ptr<ExtensionAction> page_action;

    // The extension's browser action, if any.
    scoped_ptr<ExtensionAction> browser_action;

    // Optional list of NPAPI plugins and associated properties.
    std::vector<PluginInfo> plugins;

    // Optional URL to a master page of which a single instance should be always
    // loaded in the background.
    GURL background_url;

    // Optional URL to a page for setting options/preferences.
    GURL options_url;

    // Optional URL to a devtools extension page.
    GURL devtools_url;

    // Optional list of toolstrips and associated properties.
    std::vector<GURL> toolstrips;

    // The public key used to sign the contents of the crx package.
    std::string public_key;

    // A map of resource id's to relative file paths.
    scoped_ptr<DictionaryValue> theme_images;

    // A map of color names to colors.
    scoped_ptr<DictionaryValue> theme_colors;

    // A map of color names to colors.
    scoped_ptr<DictionaryValue> theme_tints;

    // A map of display properties.
    scoped_ptr<DictionaryValue> theme_display_properties;

    // Whether the extension is a theme.
    bool is_theme;

    // The sites this extension has permission to talk to (using XHR, etc).
    URLPatternList host_permissions;

    // URL for fetching an update manifest
    GURL update_url;

    // A copy of the manifest that this extension was created from.
    scoped_ptr<DictionaryValue> manifest_value;

    // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
    // which override the handling of those URLs.
    URLOverrideMap chrome_url_overrides;

    // Whether this extension uses app features.
    bool is_app;

    // The local path inside the extension to use with the launcher.
    std::string launch_local_path;

    // A web url to use with the launcher. Note that this might be relative or
    // absolute. If relative, it is relative to web_origin.
    std::string launch_web_url;

    // The type of container to launch into.
    extension_misc::LaunchContainer launch_container;

    // The default size of the container when launching. Only respected for
    // containers like panels and windows.
    int launch_width;
    int launch_height;

    // The Omnibox keyword for this extension, or empty if there is none.
    std::string omnibox_keyword;

   protected:
    friend class base::RefCountedThreadSafe<StaticData>;
    ~StaticData();
  };

  // Contains the subset of the extension's (private) data that can be modified
  // after initialization. This class should only be accessed on the UI thread.
  struct RuntimeData {
    // We keep a cache of images loaded from extension resources based on their
    // path and a string representation of a size that may have been used to
    // scale it (or the empty string if the image is at its original size).
    typedef std::pair<FilePath, std::string> ImageCacheKey;
    typedef std::map<ImageCacheKey, SkBitmap> ImageCache;

    RuntimeData();
    ~RuntimeData();

    // True if the background page is ready.
    bool background_page_ready;

    // True while the extension is being upgraded.
    bool being_upgraded;

    // Cached images for this extension.
    ImageCache image_cache_;
  };

  // A permission is defined by its |name| (what is used in the manifest),
  // and the |message_id| that's used by install/update UI.
  struct Permission {
    const char* const name;
    const int message_id;
  };

  // The install message id for |permission|.  Returns 0 if none exists.
  static int GetPermissionMessageId(const std::string& permission);

  // Returns the full list of permission messages that this extension
  // should display at install time.
  std::vector<string16> GetPermissionMessages();

  // Returns the distinct hosts that should be displayed in the install UI. This
  // discards some of the detail that is present in the manifest to make it as
  // easy as possible to process by users. In particular we disregard the scheme
  // and path components of URLPatterns and de-dupe the result.
  static std::vector<std::string> GetDistinctHosts(
      const URLPatternList& host_patterns);
  std::vector<std::string> GetDistinctHosts();

  // Icon sizes used by the extension system.
  static const int kIconSizes[];

  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;

  // Each permission is a module that the extension is permitted to use.
  //
  // NOTE: To add a new permission, define it here, and add an entry to
  // Extension::kPermissions.
  static const char kBackgroundPermission[];
  static const char kBookmarkPermission[];
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

  explicit Extension(const FilePath& path);
  virtual ~Extension();

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
           location == Extension::EXTERNAL_PREF_DOWNLOAD;
  }

  // See HistogramType definition above.
  HistogramType GetHistogramType();

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

  // Returns the url prefix for the extension/apps gallery. Can be set via the
  // --apps-gallery-url switch. The URL returned will not contain a trailing
  // slash. Do not use this as a prefix/extent for the store.  Instead see
  // ExtensionsService::GetWebStoreApp or
  // ExtensionsService::IsDownloadFromGallery
  static std::string ChromeStoreLaunchURL();

  // Helper function that consolidates the check for whether the script can
  // execute into one location. |page_url| is the page that is the candidate
  // for running the script, |can_execute_script_everywhere| specifies whether
  // the extension is on the whitelist, |allowed_pages| is a vector of
  // URLPatterns, listing what access the extension has, |script| is the script
  // pointer (if content script) and |error| is an optional parameter, which
  // will receive the error string listing why access was denied.
  static bool CanExecuteScriptOnPage(
      const GURL& page_url,
      bool can_execute_script_everywhere,
      const std::vector<URLPattern>* allowed_pages,
      UserScript* script,
      std::string* error);

  // Adds an extension to the scripting whitelist. Used for testing only.
  static void SetScriptingWhitelist(const ScriptingWhitelist& whitelist);

  // Initialize the extension from a parsed manifest.
  // Usually, the id of an extension is generated by the "key" property of
  // its manifest, but if |require_key| is |false|, a temporary ID will be
  // generated based on the path.
  bool InitFromValue(const DictionaryValue& value, bool require_key,
                     std::string* error);

  const StaticData* static_data() const { return static_data_; }

  const FilePath& path() const { return static_data_->path; }
  const GURL& url() const { return static_data_->extension_url; }
  Location location() const { return static_data_->location; }
  void set_location(Location location) {
    mutable_static_data_->location = location;
  }

  const std::string& id() const { return static_data_->id; }
  const Version* version() const { return static_data_->version.get(); }
  // String representation of the version number.
  const std::string VersionString() const;
  const std::string& name() const { return static_data_->name; }
  const std::string& public_key() const { return static_data_->public_key; }
  const std::string& description() const { return static_data_->description; }
  bool converted_from_user_script() const {
    return static_data_->converted_from_user_script;
  }
  const UserScriptList& content_scripts() const {
    return static_data_->content_scripts;
  }
  ExtensionAction* page_action() const {
    return static_data_->page_action.get();
  }
  ExtensionAction* browser_action() const {
    return static_data_->browser_action.get();
  }
  const std::vector<PluginInfo>& plugins() const {
    return static_data_->plugins;
  }
  const GURL& background_url() const { return static_data_->background_url; }
  const GURL& options_url() const { return static_data_->options_url; }
  const GURL& devtools_url() const { return static_data_->devtools_url; }
  const std::vector<GURL>& toolstrips() const {
    return static_data_->toolstrips;
  }
  const std::set<std::string>& api_permissions() const {
    return static_data_->api_permissions;
  }
  const URLPatternList& host_permissions() const {
    return static_data_->host_permissions;
  }

  // Returns true if the extension has the specified API permission.
  static bool HasApiPermission(const std::set<std::string>& api_permissions,
                               const std::string& function_name);

  bool HasApiPermission(const std::string& function_name) const {
    return HasApiPermission(this->api_permissions(), function_name);
  }

  const ExtensionExtent& GetEffectiveHostPermissions() const {
    return static_data_->effective_host_permissions;
  }

  // Whether or not the extension is allowed permission for a URL pattern from
  // the manifest.  http, https, and chrome://favicon/ is allowed for all
  // extensions, while component extensions are allowed access to
  // chrome://resources.
  bool CanSpecifyHostPermission(const URLPattern pattern) const;

  // Whether the extension has access to the given URL.
  bool HasHostPermission(const GURL& url) const;

  // Whether the extension has effective access to all hosts. This is true if
  // there is a content script that matches all hosts, if there is a host
  // permission grants access to all hosts (like <all_urls>) or an api
  // permission that effectively grants access to all hosts (e.g. proxy,
  // network, etc.)
  bool HasEffectiveAccessToAllHosts() const;

  const GURL& update_url() const { return static_data_->update_url; }

  const ExtensionIconSet& icons() const {
    return static_data_->icons;
  }

  // Returns the Google Gallery URL for this extension, if one exists. For
  // third-party extensions, this returns a blank GURL.
  GURL GalleryUrl() const;

  // Theme-related.
  DictionaryValue* GetThemeImages() const {
    return static_data_->theme_images.get();
  }
  DictionaryValue* GetThemeColors() const {
    return static_data_->theme_colors.get();
  }
  DictionaryValue* GetThemeTints() const {
    return static_data_->theme_tints.get();
  }
  DictionaryValue* GetThemeDisplayProperties() const {
    return static_data_->theme_display_properties.get();
  }
  bool is_theme() const { return static_data_->is_theme; }

  // Returns a list of paths (relative to the extension dir) for images that
  // the browser might load (like themes and page action icons).
  std::set<FilePath> GetBrowserImages();

  // Get an extension icon as a resource or URL.
  ExtensionResource GetIconResource(int size,
                                    ExtensionIconSet::MatchType match_type);
  GURL GetIconURL(int size, ExtensionIconSet::MatchType match_type);

  const DictionaryValue* manifest_value() const {
    return static_data_->manifest_value.get();
  }

  const std::string default_locale() const {
    return static_data_->default_locale;
  }

  // Chrome URL overrides (see ExtensionOverrideUI).
  const URLOverrideMap& GetChromeURLOverrides() const {
    return static_data_->chrome_url_overrides;
  }

  const std::string omnibox_keyword() const {
    return static_data_->omnibox_keyword;
  }

  bool is_app() const { return static_data_->is_app; }
  const ExtensionExtent& web_extent() const {
    return static_data_->extent;
  }
  const std::string& launch_local_path() const {
    return static_data_->launch_local_path;
  }
  const std::string& launch_web_url() const {
    return static_data_->launch_web_url;
  }
  extension_misc::LaunchContainer launch_container() const {
    return static_data_->launch_container;
  }
  int launch_width() const { return static_data_->launch_width; }
  int launch_height() const { return static_data_->launch_height; }
  bool incognito_split_mode() const {
    return static_data_->incognito_split_mode;
  }

  // Gets the fully resolved absolute launch URL.
  GURL GetFullLaunchURL() const;

  // Whether the background page, if any, is ready. We don't load other
  // components until then. If there is no background page, we consider it to
  // be ready.
  bool GetBackgroundPageReady();
  void SetBackgroundPageReady();

  // Getter and setter for the flag that specifies whether the extension is
  // being upgraded.
  bool being_upgraded() const { return GetRuntimeData()->being_upgraded; }
  void set_being_upgraded(bool value) {
    GetRuntimeData()->being_upgraded = value;
  }

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
  bool is_hosted_app() const { return is_app() && !web_extent().is_empty(); }
  bool is_packaged_app() const { return is_app() && web_extent().is_empty(); }

  // Returns true if this extension is a COMPONENT extension, or if it is
  // on the whitelist of extensions that can script all pages.
  bool CanExecuteScriptEverywhere() const;

 private:
  // Normalize the path for use by the extension. On Windows, this will make
  // sure the drive letter is uppercase.
  static FilePath MaybeNormalizePath(const FilePath& path);

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

  // Calculates the effective host permissions from the permissions and content
  // script petterns.
  void InitEffectiveHostPermissions();

  // Returns true if the extension has more than one "UI surface". For example,
  // an extension that has a browser action and a page action.
  bool HasMultipleUISurfaces() const;

  // Figures out if a source contains keys not associated with themes - we
  // don't want to allow scripts and such to be bundled with themes.
  bool ContainsNonThemeKeys(const DictionaryValue& source);

  // Returns true if the string is one of the known api permissions (see
  // kPermissions).
  bool IsAPIPermission(const std::string& permission);

  // The set of unique API install messages that the extension has.
  // NOTE: This only includes messages related to permissions declared in the
  // "permissions" key in the manifest.  Permissions implied from other features
  // of the manifest, like plugins and content scripts are not included.
  std::set<string16> GetSimplePermissionMessages();

  // The permission message displayed related to the host permissions for
  // this extension.
  string16 GetHostPermissionMessage();

  // Returns a mutable pointer to our runtime data. Can only be called on
  // the UI thread.
  RuntimeData* GetRuntimeData() const;

  // Collection of extension data that doesn't change doesn't change once an
  // Extension object has been initialized. The mutable version is valid only
  // until InitFromValue finishes, to ensure we don't accidentally modify it
  // post-initialization.
  StaticData* mutable_static_data_;
  scoped_refptr<const StaticData> static_data_;

  // Runtime data.
  scoped_ptr<const RuntimeData> runtime_data_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, LoadPageActionHelper);
  FRIEND_TEST_ALL_PREFIXES(TabStripModelTest, Apps);

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

typedef std::vector<Extension*> ExtensionList;
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
  // TODO(akalin): Once we have a unified ExtensionType, replace the
  // below member variables with a member of that type.
  bool is_theme;
  bool is_app;
  bool converted_from_user_script;
  GURL update_url;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
