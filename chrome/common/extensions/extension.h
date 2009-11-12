// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

// Represents a Chrome extension.
class Extension {
 public:
  typedef std::vector<URLPattern> HostPermissions;
  typedef std::map<const std::string, GURL> URLOverrideMap;

  // What an extension was loaded from.
  enum Location {
    INVALID,
    INTERNAL,           // A crx file from the internal Extensions directory.
    EXTERNAL_PREF,      // A crx file from an external directory (via prefs).
    EXTERNAL_REGISTRY,  // A crx file from an external directory (via eg the
                        // registry on Windows).
    LOAD                // --load-extension.
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
    EXTENSION_ICON_BITTY = 16,
  };

  // The URL to browse the extensions gallery.
  static const char* kGalleryBrowseUrl;

  // Icon sizes used by the extension system.
  static const int kIconSizes[];

  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;

  // Each permission is a module that the extension is permitted to use.
  static const char* kTabPermission;
  static const char* kBookmarkPermission;

  static const char* kPermissionNames[];
  static const size_t kNumPermissions;

  // Experimental extension APIs are guarded by the following flag.
  static const char* kExperimentalName;

  struct PrivacyBlacklistInfo {
    FilePath path;  // Path to the plain-text blacklist.
  };

  // An NPAPI plugin included in the extension.
  struct PluginInfo {
    FilePath path;  // Path to the plugin.
    bool is_public;  // False if only this extension can load this plugin.
  };

  // A toolstrip and its associated mole.
  struct ToolstripInfo {
    ToolstripInfo() : mole_height(0) {}

    GURL toolstrip;
    GURL mole;
    int mole_height;
  };

  // The name of the manifest inside an extension.
  static const char kManifestFilename[];

  // The name of locale folder inside an extension.
  static const char kLocaleFolder[];

  // The name of the messages file inside an extension.
  static const char kMessagesFilename[];

#if defined(OS_WIN)
  static const char* kExtensionRegistryPath;
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
           location == Extension::EXTERNAL_REGISTRY;
  }

  // Returns an absolute url to a resource inside of an extension. The
  // |extension_url| argument should be the url() from an Extension object. The
  // |relative_path| can be untrusted user input. The returned URL will either
  // be invalid() or a child of |extension_url|.
  // NOTE: Static so that it can be used from multiple threads.
  static GURL GetResourceURL(const GURL& extension_url,
                             const std::string& relative_path);
  GURL GetResourceURL(const std::string& relative_path) {
    return GetResourceURL(url(), relative_path);
  }

  // Returns an extension resource object. The |extension_path| argument should
  // be the path() from an Extension object.
  // The |relative_path| can be untrusted user input.
  // NOTE: Static so that it can be used from multiple threads.
  static ExtensionResource GetResource(const FilePath& extension_path,
                                       const std::string& relative_path);
  ExtensionResource GetResource(const std::string& relative_path) {
    return GetResource(path(), relative_path);
  }

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

  // Initialize the extension from a parsed manifest.
  // If |require_id| is true, will return an error if the "id" key is missing
  // from the value.
  bool InitFromValue(const DictionaryValue& value, bool require_id,
                     std::string* error);

  const FilePath& path() const { return path_; }
  void set_path(const FilePath& path) { path_ = path; }
  const GURL& url() const { return extension_url_; }
  const Location location() const { return location_; }
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
  const std::vector<PrivacyBlacklistInfo>& privacy_blacklists() const {
    return privacy_blacklists_;
  }
  const std::vector<PluginInfo>& plugins() const { return plugins_; }
  const GURL& background_url() const { return background_url_; }
  const GURL& options_url() const { return options_url_; }
  const std::vector<ToolstripInfo>& toolstrips() const { return toolstrips_; }
  const std::vector<std::string>& api_permissions() const {
    return api_permissions_;
  }
  const HostPermissions& host_permissions() const {
    return host_permissions_;
  }

  // Returns true if the extension has permission to access the host for the
  // specified URL.
  bool CanAccessHost(const GURL& url) const;

  // Returns true if the extension has the specified API permission.
  bool HasApiPermission(const std::string& permission) const {
    return std::find(api_permissions_.begin(), api_permissions_.end(),
                     permission) != api_permissions_.end();
  }

  // Returns the set of hosts that the extension effectively has access to. This
  // is used in the permissions UI and is a combination of the hosts accessible
  // through content scripts and the hosts accessible through XHR.
  const std::set<std::string> GetEffectiveHostPermissions() const;

  // Whether the extension has access to all hosts. This is true if there is
  // a content script that matches all hosts, or if there is a host permission
  // for all hosts.
  bool HasAccessToAllHosts() const;

  const GURL& update_url() const { return update_url_; }
  const std::map<int, std::string>& icons() const { return icons_; }

  // Returns the origin of this extension. This function takes a |registry_path|
  // so that the registry location can be overwritten during testing.
  Location ExternalExtensionInstallType(std::string registry_path);

  // Theme-related.
  DictionaryValue* GetThemeImages() const { return theme_images_.get(); }
  DictionaryValue* GetThemeColors() const { return theme_colors_.get(); }
  DictionaryValue* GetThemeTints() const { return theme_tints_.get(); }
  DictionaryValue* GetThemeDisplayProperties() const {
    return theme_display_properties_.get();
  }
  bool IsTheme() const { return is_theme_; }

  // Returns a list of paths (relative to the extension dir) for images that
  // the browser might load (like themes and page action icons).
  std::set<FilePath> GetBrowserImages();

  // Returns an absolute path to the given icon inside of the extension. Returns
  // an empty FilePath if the extension does not have that icon.
  ExtensionResource GetIconPath(Icons icon);

  const DictionaryValue* manifest_value() const {
    return manifest_value_.get();
  }

  // Getter/setter for l10n message bundle.
  const ExtensionMessageBundle* message_bundle() const {
    return message_bundle_.get();
  }
  void set_message_bundle(ExtensionMessageBundle* message_bundle) {
    message_bundle_.reset(message_bundle);
  }

  // Chrome URL overrides (see ExtensionOverrideUI).
  const URLOverrideMap& GetChromeURLOverrides() const {
    return chrome_url_overrides_;
  }

  // Runtime data:
  // Put dynamic data about the state of a running extension below.

  // Whether the background page, if any, is ready. We don't load other
  // components until then. If there is no background page, we consider it to
  // be ready.
  bool GetBackgroundPageReady();
  void SetBackgroundPageReady();

 private:
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
                       const wchar_t* globs_property_name,
                       std::string* error,
                       void (UserScript::*add_method) (const std::string& glob),
                       UserScript *instance);

  // Helper method to load an ExtensionAction from the page_action or
  // browser_action entries in the manifest.
  ExtensionAction* LoadExtensionActionHelper(
      const DictionaryValue* extension_action, std::string* error);

  // Figures out if a source contains keys not associated with themes - we
  // don't want to allow scripts and such to be bundled with themes.
  bool ContainsNonThemeKeys(const DictionaryValue& source);

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

  // The extension's human-readable name.
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

  // Optional list of privacy blacklistrom.
  std::vector<PrivacyBlacklistInfo> privacy_blacklists_;

  // Optional list of NPAPI plugins and associated properties.
  std::vector<PluginInfo> plugins_;

  // Optional URL to a master page of which a single instance should be always
  // loaded in the background.
  GURL background_url_;

  // Optional URL to a page for setting options/preferences.
  GURL options_url_;

  // Optional list of toolstrips_ and associated properties.
  std::vector<ToolstripInfo> toolstrips_;

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
  HostPermissions host_permissions_;

  // The paths to the icons the extension contains mapped by their width.
  std::map<int, std::string> icons_;

  // URL for fetching an update manifest
  GURL update_url_;

  // A copy of the manifest that this extension was created from.
  scoped_ptr<DictionaryValue> manifest_value_;

  // Handles the l10n messages replacement and parsing.
  scoped_ptr<ExtensionMessageBundle> message_bundle_;

  // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
  // which override the handling of those URLs.
  URLOverrideMap chrome_url_overrides_;

  // Runtime data:

  // True if the background page is ready.
  bool background_page_ready_;

  FRIEND_TEST(ExtensionTest, LoadPageActionHelper);

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
