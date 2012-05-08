// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_commands.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "googleurl/src/gurl.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/size.h"

class ExtensionResource;
class FileBrowserHandler;
class SkBitmap;
class Version;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class Manifest;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

// Represents a Chrome extension.
class Extension : public base::RefCountedThreadSafe<Extension> {
 public:
  typedef std::map<const std::string, GURL> URLOverrideMap;
  typedef std::vector<std::string> ScriptingWhitelist;
  typedef std::vector<linked_ptr<FileBrowserHandler> > FileBrowserHandlerList;

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
    // An external extension that the user uninstalled. We should not reinstall
    // such extensions on startup.
    EXTERNAL_EXTENSION_UNINSTALLED,
    NUM_STATES
  };

  // Used to record the reason an extension was disabled.
  enum DisableReason {
    DISABLE_UNKNOWN,
    DISABLE_USER_ACTION,
    DISABLE_PERMISSIONS_INCREASE,
    DISABLE_RELOAD,
    DISABLE_LAST,  // Not used.
  };

  enum InstallType {
    INSTALL_ERROR,
    DOWNGRADE,
    REINSTALL,
    UPGRADE,
    NEW_INSTALL
  };

  // Do not change the order of entries or remove entries in this list
  // as this is used in UMA_HISTOGRAM_ENUMERATIONs about extensions.
  enum Type {
    TYPE_UNKNOWN = 0,
    TYPE_EXTENSION,
    TYPE_THEME,
    TYPE_USER_SCRIPT,
    TYPE_HOSTED_APP,
    TYPE_PACKAGED_APP,
    TYPE_PLATFORM_APP
  };

  enum SyncType {
    SYNC_TYPE_NONE = 0,
    SYNC_TYPE_EXTENSION,
    SYNC_TYPE_APP
  };

  // An NPAPI plugin included in the extension.
  struct PluginInfo {
    FilePath path;  // Path to the plugin.
    bool is_public;  // False if only this extension can load this plugin.
  };

  // An NaCl module included in the extension.
  struct NaClModuleInfo {
    GURL url;
    std::string mime_type;
  };

  enum InputComponentType {
    INPUT_COMPONENT_TYPE_NONE = -1,
    INPUT_COMPONENT_TYPE_IME,
    INPUT_COMPONENT_TYPE_VIRTUAL_KEYBOARD,
    INPUT_COMPONENT_TYPE_COUNT
  };

  struct InputComponentInfo {
    // Define out of line constructor/destructor to please Clang.
    InputComponentInfo();
    ~InputComponentInfo();

    std::string name;
    InputComponentType type;
    std::string id;
    std::string description;
    std::string language;
    std::set<std::string> layouts;
    std::string shortcut_keycode;
    bool shortcut_alt;
    bool shortcut_ctrl;
    bool shortcut_shift;
  };

  struct TtsVoice {
    // Define out of line constructor/destructor to please Clang.
    TtsVoice();
    ~TtsVoice();

    std::string voice_name;
    std::string lang;
    std::string gender;
    std::set<std::string> event_types;
  };

  // OAuth2 info included in the extension.
  struct OAuth2Info {
    OAuth2Info();
    ~OAuth2Info();

    ExtensionOAuth2Scopes GetScopesAsSet();

    std::string client_id;
    std::vector<std::string> scopes;
  };

  enum InitFromValueFlags {
    NO_FLAGS = 0,

    // Usually, the id of an extension is generated by the "key" property of
    // its manifest, but if |REQUIRE_KEY| is not set, a temporary ID will be
    // generated based on the path.
    REQUIRE_KEY = 1 << 0,

    // Requires the extension to have an up-to-date manifest version.
    // Typically, we'll support multiple manifest versions during a version
    // transition.  This flag signals that we want to require the most modern
    // manifest version that Chrome understands.
    REQUIRE_MODERN_MANIFEST_VERSION = 1 << 1,

    // |STRICT_ERROR_CHECKS| enables extra error checking, such as
    // checks that URL patterns do not contain ports.  This error
    // checking may find an error that a previous version of
    // Chrome did not flag.  To avoid errors in installed extensions
    // when Chrome is upgraded, strict error checking is only enabled
    // when loading extensions as a developer would (such as loading
    // an unpacked extension), or when loading an extension that is
    // tied to a specific version of Chrome (such as a component
    // extension).  Most callers will set the |STRICT_ERROR_CHECKS| bit when
    // Extension::ShouldDoStrictErrorChecking(location) returns true.
    STRICT_ERROR_CHECKS = 1 << 2,

    // |ALLOW_FILE_ACCESS| indicates that the user is allowing this extension
    // to have file access. If it's not present, then permissions and content
    // scripts that match file:/// URLs will be filtered out.
    ALLOW_FILE_ACCESS = 1 << 3,

    // |FROM_WEBSTORE| indicates that the extension was installed from the
    // Chrome Web Store.
    FROM_WEBSTORE = 1 << 4,

    // |FROM_BOOKMARK| indicates the extension was created using a mock App
    // created from a bookmark.
    FROM_BOOKMARK = 1 << 5,

    // |FOLLOW_SYMLINKS_ANYWHERE| means that resources can be symlinks to
    // anywhere in the filesystem, rather than being restricted to the
    // extension directory.
    FOLLOW_SYMLINKS_ANYWHERE = 1 << 6,
  };

  static scoped_refptr<Extension> Create(const FilePath& path,
                                         Location location,
                                         const base::DictionaryValue& value,
                                         int flags,
                                         std::string* error);

  // In a few special circumstances, we want to create an Extension and give it
  // an explicit id. Most consumers should just use the other Create() method.
  static scoped_refptr<Extension> Create(const FilePath& path,
      Location location,
      const base::DictionaryValue& value,
      int flags,
      const std::string& explicit_id,
      std::string* error);

  // Given two install sources, return the one which should take priority
  // over the other. If an extension is installed from two sources A and B,
  // its install source should be set to GetHigherPriorityLocation(A, B).
  static Location GetHigherPriorityLocation(Location loc1, Location loc2);

  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;

  // Valid schemes for web extent URLPatterns.
  static const int kValidWebExtentSchemes;

  // Valid schemes for host permission URLPatterns.
  static const int kValidHostPermissionSchemes;

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
  // Used while developing extensions, before they have a key.
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

  // Whether extensions with |location| can be uninstalled or not. Policy
  // controlled extensions are silently auto-installed and updated, and cannot
  // be disabled by the user. The same applies for internal components.
  static inline bool UserMayDisable(Location location) {
    return location != Extension::EXTERNAL_POLICY_DOWNLOAD &&
           location != Extension::COMPONENT;
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

  // Unpacked extensions start off with file access since they are a developer
  // feature.
  static inline bool ShouldAlwaysAllowFileAccess(Location location) {
    return location == Extension::LOAD;
  }

  // Fills the |info| dictionary with basic information about the extension.
  // |enabled| is injected for easier testing.
  void GetBasicInfo(bool enabled, base::DictionaryValue* info) const;

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

  // Returns true if the specified resource is web accessible.
  bool IsResourceWebAccessible(const std::string& relative_path) const;

  // Returns true when 'web_accessible_resources' are defined for the extension.
  bool HasWebAccessibleResources() const;

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
  static bool GenerateId(const std::string& input,
                         std::string* output) WARN_UNUSED_RESULT;

  // Expects base64 encoded |input| and formats into |output| including
  // the appropriate header & footer.
  static bool FormatPEMForFileOutput(const std::string& input,
                                     std::string* output,
                                     bool is_public);

  // Given an extension, icon size, and match type, read a valid icon if present
  // and decode it into result. In the browser process, this will DCHECK if not
  // called on the file thread. To easily load extension images on the UI
  // thread, see ImageLoadingTracker.
  static void DecodeIcon(const Extension* extension,
                         ExtensionIconSet::Icons icon_size,
                         ExtensionIconSet::MatchType match_type,
                         scoped_ptr<SkBitmap>* result);

  // Given an extension and icon size, read it if present and decode it into
  // result. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoadingTracker.
  static void DecodeIcon(const Extension* extension,
                         ExtensionIconSet::Icons icon_size,
                         scoped_ptr<SkBitmap>* result);

  // Given an icon_path and icon size, read it if present and decode it into
  // result. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoadingTracker.
  static void DecodeIconFromPath(const FilePath& icon_path,
                                 ExtensionIconSet::Icons icon_size,
                                 scoped_ptr<SkBitmap>* result);

  // Returns the default extension/app icon (for extensions or apps that don't
  // have one).
  static const SkBitmap& GetDefaultIcon(bool is_app);

  // Returns the base extension url for a given |extension_id|.
  static GURL GetBaseURLFromExtensionId(const std::string& extension_id);

  // Adds an extension to the scripting whitelist. Used for testing only.
  static void SetScriptingWhitelist(const ScriptingWhitelist& whitelist);
  static const ScriptingWhitelist* GetScriptingWhitelist();

  // Parses the host and api permissions from the specified permission |key|
  // from |manifest_|.
  bool ParsePermissions(const char* key,
                        string16* error,
                        ExtensionAPIPermissionSet* api_permissions,
                        URLPatternSet* host_permissions);

  bool HasAPIPermission(ExtensionAPIPermission::ID permission) const;
  bool HasAPIPermission(const std::string& function_name) const;

  const URLPatternSet& GetEffectiveHostPermissions() const;

  // Returns true if the extension can silently increase its permission level.
  // Users must approve permissions for unpacked and packed extensions in the
  // following situations:
  //  - when installing or upgrading packed extensions
  //  - when installing unpacked extensions that have NPAPI plugins
  //  - when either type of extension requests optional permissions
  bool CanSilentlyIncreasePermissions() const;

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

  // Returns the full list of permission messages that this extension
  // should display at install time.
  ExtensionPermissionMessages GetPermissionMessages() const;

  // Returns the full list of permission messages that this extension
  // should display at install time. The messages are returned as strings
  // for convenience.
  std::vector<string16> GetPermissionMessageStrings() const;

  // Sets the active |permissions|.
  void SetActivePermissions(const ExtensionPermissionSet* permissions) const;

  // Gets the extension's active permission set.
  scoped_refptr<const ExtensionPermissionSet> GetActivePermissions() const;

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
                              const UserScript* script,
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

  // Returns the sync bucket to use for this extension.
  SyncType GetSyncType() const;

  // Returns true if the extension should be synced.
  bool IsSyncable() const;

  // Returns true if the extension should be displayed in the launcher.
  bool ShouldDisplayInLauncher() const;

  // Returns true if the extension should be displayed in the extension
  // settings page (i.e. chrome://extensions).
  bool ShouldDisplayInExtensionSettings() const;

  // Accessors:

  const FilePath& path() const { return path_; }
  const GURL& url() const { return extension_url_; }
  Location location() const;
  const std::string& id() const;
  const Version* version() const { return version_.get(); }
  const std::string VersionString() const;
  const std::string& name() const { return name_; }
  const std::string public_key() const { return public_key_; }
  const std::string& description() const { return description_; }
  int manifest_version() const { return manifest_version_; }
  bool converted_from_user_script() const {
    return converted_from_user_script_;
  }
  const UserScriptList& content_scripts() const { return content_scripts_; }
  ExtensionAction* page_action() const { return page_action_.get(); }
  ExtensionAction* browser_action() const { return browser_action_.get(); }
  ExtensionAction::Type declared_action_type() const {
    return declared_action_type_;
  }
  const FileBrowserHandlerList* file_browser_handlers() const {
    return file_browser_handlers_.get();
  }
  const std::vector<PluginInfo>& plugins() const { return plugins_; }
  const std::vector<NaClModuleInfo>& nacl_modules() const {
    return nacl_modules_;
  }
  const std::vector<InputComponentInfo>& input_components() const {
    return input_components_;
  }
  // The browser action command that the extension wants to use, which is not
  // necessarily the one it can use, as it might be inactive (see also
  // GetActiveBrowserActionCommand in ExtensionKeybindingRegistry).
  const extensions::Command* browser_action_command() const {
    return browser_action_command_.get();
  }
  // The page action command that the extension wants to use, which is not
  // necessarily the one it can use, as it might be inactive (see also
  // GetActivePageActionCommand in ExtensionKeybindingRegistry).
  const extensions::Command* page_action_command() const {
    return page_action_command_.get();
  }
  // The map (of command names to commands) that the extension wants to use,
  // which is not necessarily the one it can use, as they might be inactive
  // (see also GetActiveNamedCommands in ExtensionKeybindingRegistry).
  const extensions::CommandMap& named_commands() const {
    return named_commands_;
  }
  bool has_background_page() const {
    return background_url_.is_valid() || !background_scripts_.empty();
  }
  bool allow_background_js_access() const {
    return allow_background_js_access_;
  }
  const std::vector<std::string>& background_scripts() const {
    return background_scripts_;
  }
  bool has_persistent_background_page() const {
    return has_background_page() && background_page_is_persistent_;
  }
  bool has_lazy_background_page() const {
    return has_background_page() && !background_page_is_persistent_;
  }
  const GURL& options_url() const { return options_url_; }
  const GURL& devtools_url() const { return devtools_url_; }
  const ExtensionPermissionSet* optional_permission_set() const {
    return optional_permission_set_.get();
  }
  const ExtensionPermissionSet* required_permission_set() const {
    return required_permission_set_.get();
  }
  const std::vector<std::string>& install_warnings() const {
    return install_warnings_;
  }
  const GURL& update_url() const { return update_url_; }
  const ExtensionIconSet& icons() const { return icons_; }
  const extensions::Manifest* manifest() const {
    return manifest_;
  }
  const std::string default_locale() const { return default_locale_; }
  const URLOverrideMap& GetChromeURLOverrides() const {
    return chrome_url_overrides_;
  }
  const std::string omnibox_keyword() const { return omnibox_keyword_; }
  bool incognito_split_mode() const { return incognito_split_mode_; }
  bool offline_enabled() const { return offline_enabled_; }
  const std::vector<TtsVoice>& tts_voices() const { return tts_voices_; }
  const OAuth2Info& oauth2_info() const { return oauth2_info_; }
  const std::vector<webkit_glue::WebIntentServiceData>&
      intents_services() const {
    return intents_services_;
  }

  bool wants_file_access() const { return wants_file_access_; }
  int creation_flags() const { return creation_flags_; }
  bool from_webstore() const { return (creation_flags_ & FROM_WEBSTORE) != 0; }
  bool from_bookmark() const { return (creation_flags_ & FROM_BOOKMARK) != 0; }

  const std::string& content_security_policy() const {
    return content_security_policy_;
  }

  // App-related.
  bool is_app() const {
    return is_packaged_app() || is_hosted_app() || is_platform_app();
  }
  bool is_platform_app() const;
  bool is_hosted_app() const;
  bool is_packaged_app() const;
  bool is_storage_isolated() const { return is_storage_isolated_; }
  const URLPatternSet& web_extent() const { return extent_; }
  const std::string& launch_local_path() const { return launch_local_path_; }
  const std::string& launch_web_url() const { return launch_web_url_; }
  extension_misc::LaunchContainer launch_container() const {
    return launch_container_;
  }
  int launch_width() const { return launch_width_; }
  int launch_height() const { return launch_height_; }

  // Theme-related.
  bool is_theme() const;
  base::DictionaryValue* GetThemeImages() const { return theme_images_.get(); }
  base::DictionaryValue* GetThemeColors() const {return theme_colors_.get(); }
  base::DictionaryValue* GetThemeTints() const { return theme_tints_.get(); }
  base::DictionaryValue* GetThemeDisplayProperties() const {
    return theme_display_properties_.get();
  }

  GURL GetBackgroundURL() const;

 private:
  friend class base::RefCountedThreadSafe<Extension>;

  // We keep a cache of images loaded from extension resources based on their
  // path and a string representation of a size that may have been used to
  // scale it (or the empty string if the image is at its original size).
  typedef std::pair<FilePath, std::string> ImageCacheKey;
  typedef std::map<ImageCacheKey, SkBitmap> ImageCache;

  class RuntimeData {
   public:
    RuntimeData();
    explicit RuntimeData(const ExtensionPermissionSet* active);
    ~RuntimeData();

    void SetActivePermissions(const ExtensionPermissionSet* active);
    scoped_refptr<const ExtensionPermissionSet> GetActivePermissions() const;

   private:
    friend class base::RefCountedThreadSafe<RuntimeData>;
    scoped_refptr<const ExtensionPermissionSet> active_permissions_;
  };

  // Chooses the extension ID for an extension based on a variety of criteria.
  // The chosen ID will be set in |manifest|.
  static bool InitExtensionID(extensions::Manifest* manifest,
                              const FilePath& path,
                              const std::string& explicit_id,
                              int creation_flags,
                              string16* error);

  // Normalize the path for use by the extension. On Windows, this will make
  // sure the drive letter is uppercase.
  static FilePath MaybeNormalizePath(const FilePath& path);

  // Returns true if this extension id is from a trusted provider.
  static bool IsTrustedId(const std::string& id);

  Extension(const FilePath& path, scoped_ptr<extensions::Manifest> manifest);
  ~Extension();

  // Initialize the extension from a parsed manifest.
  // TODO(aa): Rename to just Init()? There's no Value here anymore.
  // TODO(aa): It is really weird the way this class essentially contains a copy
  // of the underlying DictionaryValue in its members. We should decide to
  // either wrap the DictionaryValue and go with that only, or we should parse
  // into strong types and discard the value. But doing both is bad.
  bool InitFromValue(int flags, string16* error);

  // The following are helpers for InitFromValue to load various features of the
  // extension from the manifest.

  bool CheckMinimumChromeVersion(string16* error);
  bool LoadAppIsolation(const ExtensionAPIPermissionSet& api_permissions,
                        string16* error);

  bool LoadRequiredFeatures(string16* error);
  bool LoadName(string16* error);
  bool LoadVersion(string16* error);

  bool LoadAppFeatures(string16* error);
  bool LoadExtent(const char* key,
                  URLPatternSet* extent,
                  const char* list_error,
                  const char* value_error,
                  string16* error);
  bool LoadLaunchContainer(string16* error);
  bool LoadLaunchURL(string16* error);

  bool LoadSharedFeatures(const ExtensionAPIPermissionSet& api_permissions,
                          string16* error);
  bool LoadDescription(string16* error);
  bool LoadManifestVersion(string16* error);
  bool LoadHomepageURL(string16* error);
  bool LoadUpdateURL(string16* error);
  bool LoadIcons(string16* error);
  bool LoadCommands(string16* error);
  bool LoadPlugins(string16* error);
  bool LoadNaClModules(string16* error);
  bool LoadWebAccessibleResources(string16* error);
  bool CheckRequirements(string16* error);
  bool LoadDefaultLocale(string16* error);
  bool LoadOfflineEnabled(string16* error);
  bool LoadOptionsPage(string16* error);
  bool LoadBackgroundScripts(string16* error);
  bool LoadBackgroundPage(const ExtensionAPIPermissionSet& api_permissions,
                          string16* error);
  bool LoadBackgroundPersistent(
      const ExtensionAPIPermissionSet& api_permissions,
      string16* error);
  bool LoadBackgroundAllowJSAccess(
      const ExtensionAPIPermissionSet& api_permissions,
      string16* error);
  // Parses a single action in the manifest.
  bool LoadWebIntentAction(const std::string& action_name,
                           const base::DictionaryValue& intent_service,
                           string16* error);
  bool LoadWebIntentServices(string16* error);
  bool LoadExtensionFeatures(const ExtensionAPIPermissionSet& api_permissions,
                             string16* error);
  bool LoadDevToolsPage(string16* error);
  bool LoadInputComponents(const ExtensionAPIPermissionSet& api_permissions,
                           string16* error);
  bool LoadContentScripts(string16* error);
  bool LoadPageAction(string16* error);
  bool LoadBrowserAction(string16* error);
  void GenerateBrowserActionIfPossible();
  bool LoadFileBrowserHandlers(string16* error);
  // Helper method to load a FileBrowserHandlerList from the manifest.
  FileBrowserHandlerList* LoadFileBrowserHandlersHelper(
      const base::ListValue* extension_actions, string16* error);
  // Helper method to load an FileBrowserHandler from manifest.
  FileBrowserHandler* LoadFileBrowserHandler(
      const base::DictionaryValue* file_browser_handlers, string16* error);
  bool LoadChromeURLOverrides(string16* error);
  bool LoadOmnibox(string16* error);
  bool LoadTextToSpeechVoices(string16* error);
  bool LoadIncognitoMode(string16* error);
  bool LoadContentSecurityPolicy(string16* error);

  bool LoadThemeFeatures(string16* error);
  bool LoadThemeImages(const base::DictionaryValue* theme_value,
                       string16* error);
  bool LoadThemeColors(const base::DictionaryValue* theme_value,
                       string16* error);
  bool LoadThemeTints(const base::DictionaryValue* theme_value,
                      string16* error);
  bool LoadThemeDisplayProperties(const base::DictionaryValue* theme_value,
                                  string16* error);

  // Helper function for implementing HasCachedImage/GetCachedImage. A return
  // value of NULL means there is no matching image cached (we allow caching an
  // empty SkBitmap).
  SkBitmap* GetCachedImageImpl(const ExtensionResource& source,
                               const gfx::Size& max_size) const;

  // Helper method that loads a UserScript object from a
  // dictionary in the content_script list of the manifest.
  bool LoadUserScriptHelper(const base::DictionaryValue* content_script,
                            int definition_index,
                            string16* error,
                            UserScript* result);

  // Helper method that loads either the include_globs or exclude_globs list
  // from an entry in the content_script lists of the manifest.
  bool LoadGlobsHelper(const base::DictionaryValue* content_script,
                       int content_script_index,
                       const char* globs_property_name,
                       string16* error,
                       void(UserScript::*add_method)(const std::string& glob),
                       UserScript *instance);

  // Helper method to load an ExtensionAction from the page_action or
  // browser_action entries in the manifest.
  scoped_ptr<ExtensionAction> LoadExtensionActionHelper(
      const base::DictionaryValue* extension_action, string16* error);

  // Helper method that loads the OAuth2 info from the 'oauth2' manifest key.
  bool LoadOAuth2Info(string16* error);

  // Returns true if the extension has more than one "UI surface". For example,
  // an extension that has a browser action and a page action.
  bool HasMultipleUISurfaces() const;

  // Updates the launch URL and extents for the extension using the given
  // |override_url|.
  void OverrideLaunchUrl(const GURL& override_url);

  // Custom checks for the experimental permission that can't be expressed in
  // _permission_features.json.
  bool CanSpecifyExperimentalPermission() const;

  // Checks whether the host |pattern| is allowed for this extension, given API
  // permissions |permissions|.
  bool CanSpecifyHostPermission(const URLPattern& pattern,
      const ExtensionAPIPermissionSet& permissions) const;

  // Check that platform app features are valid. Called after InitFromValue.
  bool CheckPlatformAppFeatures(std::string* utf8_error);

  // Cached images for this extension. This should only be touched on the UI
  // thread.
  mutable ImageCache image_cache_;

  // The extension's human-readable name. Name is used for display purpose. It
  // might be wrapped with unicode bidi control characters so that it is
  // displayed correctly in RTL context.
  // NOTE: Name is UTF-8 and may contain non-ascii characters.
  std::string name_;

  // The version of this extension's manifest. We increase the manifest
  // version when making breaking changes to the extension system.
  // Version 1 was the first manifest version (implied by a lack of a
  // manifest_version attribute in the extension's manifest). We initialize
  // this member variable to 0 to distinguish the "uninitialized" case from
  // the case when we know the manifest version actually is 1.
  int manifest_version_;

  // The absolute path to the directory the extension is stored in.
  FilePath path_;

  // Default locale for fall back. Can be empty if extension is not localized.
  std::string default_locale_;

  // If true, a separate process will be used for the extension in incognito
  // mode.
  bool incognito_split_mode_;

  // Whether the extension or app should be enabled when offline.
  bool offline_enabled_;

  // Defines the set of URLs in the extension's web content.
  URLPatternSet extent_;

  // The extension runtime data.
  mutable base::Lock runtime_data_lock_;
  mutable RuntimeData runtime_data_;

  // The set of permissions the extension can request at runtime.
  scoped_refptr<const ExtensionPermissionSet> optional_permission_set_;

  // The extension's required / default set of permissions.
  scoped_refptr<const ExtensionPermissionSet> required_permission_set_;

  // Any warnings that occurred when trying to create/parse the extension.
  std::vector<std::string> install_warnings_;

  // The icons for the extension.
  ExtensionIconSet icons_;

  // The base extension url for the extension.
  GURL extension_url_;

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

  // Indicates the type of action (browser or page) that the extension declared
  // in its manifest. This doesn't imply any value for page_action or
  // browser_action, since page actions may be turned into browser actions and
  // browser actions may be generated without any action declared.
  ExtensionAction::Type declared_action_type_;

  // The extension's file browser actions, if any.
  scoped_ptr<FileBrowserHandlerList> file_browser_handlers_;

  // Optional list of NPAPI plugins and associated properties.
  std::vector<PluginInfo> plugins_;

  // Optional list of NaCl modules and associated properties.
  std::vector<NaClModuleInfo> nacl_modules_;

  // Optional list of input components and associated properties.
  std::vector<InputComponentInfo> input_components_;

  // Optional list of commands (keyboard shortcuts).
  scoped_ptr<extensions::Command> browser_action_command_;
  scoped_ptr<extensions::Command> page_action_command_;
  extensions::CommandMap named_commands_;

  // Optional list of web accessible extension resources.
  base::hash_set<std::string> web_accessible_resources_;

  // Optional URL to a master page of which a single instance should be always
  // loaded in the background.
  GURL background_url_;

  // Optional list of scripts to use to generate a background page. If this is
  // present, background_url_ will be empty and generated by GetBackgroundURL().
  std::vector<std::string> background_scripts_;

  // True if the background page should stay loaded forever; false if it should
  // load on-demand (when it needs to handle an event). Defaults to true.
  bool background_page_is_persistent_;

  // True if the background page can be scripted by pages of the app or
  // extension, in which case all such pages must run in the same process.
  // False if such pages are not permitted to script the background page,
  // allowing them to run in different processes.
  bool allow_background_js_access_;

  // Optional URL to a page for setting options/preferences.
  GURL options_url_;

  // Optional URL to a devtools extension page.
  GURL devtools_url_;

  // The public key used to sign the contents of the crx package.
  std::string public_key_;

  // A map of resource id's to relative file paths.
  scoped_ptr<base::DictionaryValue> theme_images_;

  // A map of color names to colors.
  scoped_ptr<base::DictionaryValue> theme_colors_;

  // A map of color names to colors.
  scoped_ptr<base::DictionaryValue> theme_tints_;

  // A map of display properties.
  scoped_ptr<base::DictionaryValue> theme_display_properties_;

  // The homepage for this extension. Useful if it is not hosted by Google and
  // therefore does not have a Gallery URL.
  GURL homepage_url_;

  // URL for fetching an update manifest
  GURL update_url_;

  // The manifest from which this extension was created.
  //
  // NOTE: This is an owned pointer, but can't use scoped_ptr because that would
  // require manifest.h, which would in turn create a circulate dependency
  // between extension.h and manifest.h.
  //
  // TODO(aa): Pull Extension::Type and Extension::Location out into their own
  // files so that manifest.h can rely on them and not get all of extension.h
  // too, and then change this back to a scoped_ptr.
  extensions::Manifest* manifest_;

  // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
  // which override the handling of those URLs. (see ExtensionOverrideUI).
  URLOverrideMap chrome_url_overrides_;

  // Whether this extension requests isolated storage.
  bool is_storage_isolated_;

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

  // The OAuth2 client id and scopes, if specified by the extension.
  OAuth2Info oauth2_info_;

  // List of intent services that this extension provides, if any.
  std::vector<webkit_glue::WebIntentServiceData> intents_services_;

  // Whether the extension has host permissions or user script patterns that
  // imply access to file:/// scheme URLs (the user may not have actually
  // granted it that access).
  bool wants_file_access_;

  // The flags that were passed to InitFromValue.
  int creation_flags_;

  // The Content-Security-Policy for this extension.  Extensions can use
  // Content-Security-Policies to mitigate cross-site scripting and other
  // vulnerabilities.
  std::string content_security_policy_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionTest, LoadPageActionHelper);
  FRIEND_TEST_ALL_PREFIXES(TabStripModelTest, Apps);

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

typedef std::vector< scoped_refptr<const Extension> > ExtensionList;
typedef std::set<std::string> ExtensionIdSet;

// Handy struct to pass core extension info around.
struct ExtensionInfo {
  ExtensionInfo(const base::DictionaryValue* manifest,
                const std::string& id,
                const FilePath& path,
                Extension::Location location);
  ~ExtensionInfo();

  scoped_ptr<base::DictionaryValue> extension_manifest;
  std::string extension_id;
  FilePath extension_path;
  Extension::Location extension_location;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInfo);
};

struct UnloadedExtensionInfo {
  extension_misc::UnloadedExtensionReason reason;

  // Was the extension already disabled?
  bool already_disabled;

  // The extension being unloaded - this should always be non-NULL.
  const Extension* extension;

  UnloadedExtensionInfo(
      const Extension* extension,
      extension_misc::UnloadedExtensionReason reason);
};

// The details sent for EXTENSION_PERMISSIONS_UPDATED notifications.
struct UpdatedExtensionPermissionsInfo {
  enum Reason {
    ADDED,    // The permissions were added to the extension.
    REMOVED,  // The permissions were removed from the extension.
  };

  Reason reason;

  // The extension who's permissions have changed.
  const Extension* extension;

  // The permissions that have changed. For Reason::ADDED, this would contain
  // only the permissions that have added, and for Reason::REMOVED, this would
  // only contain the removed permissions.
  const ExtensionPermissionSet* permissions;

  UpdatedExtensionPermissionsInfo(
      const Extension* extension,
      const ExtensionPermissionSet* permissions,
      Reason reason);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
