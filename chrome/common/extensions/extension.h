// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_H_

#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/page_action.h"
#include "googleurl/src/gurl.h"

// Represents a Chrome extension.
class Extension {
 public:
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
    DISABLED,
    ENABLED,
    KILLBIT,  // Don't install/upgrade (applies to external extensions only).
  };

  enum InstallType {
    DOWNGRADE,
    REINSTALL,
    UPGRADE,
    NEW_INSTALL
  };

  // Icon sizes used by the extension system.
  static const int kKnownIconSizes[];

  // An NPAPI plugin included in the extension.
  struct PluginInfo {
    FilePath path;  // Path to the plugin.
    bool is_public;  // False if only this extension can load this plugin.
  };

  // The name of the manifest inside an extension.
  static const char kManifestFilename[];

#if defined(OS_WIN)
  static const char* kExtensionRegistryPath;
#endif

  // The number of bytes in a legal id.
  static const size_t kIdSize;

  // The mimetype used for extensions.
  static const char kMimeType[];

  Extension() : location_(INVALID), is_theme_(false) {}
  explicit Extension(const FilePath& path);
  virtual ~Extension();

  // Resets the id counter. This is only useful for unit tests.
  static void ResetGeneratedIdCounter() {
    id_counter_ = 0;
  }

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

  // Returns an absolute path to a resource inside of an extension. The
  // |extension_path| argument should be the path() from an Extension object.
  // The |relative_path| can be untrusted user input. The returned path will
  // either be empty or a child of extension_path.
  // NOTE: Static so that it can be used from multiple threads.
  static FilePath GetResourcePath(const FilePath& extension_path,
                                  const std::string& relative_path);
  FilePath GetResourcePath(const std::string& relative_path) {
    return GetResourcePath(path(), relative_path);
  }

  // |input| is expected to be the text of an rsa public or private key. It
  // tolerates the presence or absence of bracking header/footer like this:
  //     -----(BEGIN|END) [RSA PUBLIC/PRIVATE] KEY-----
  // and may contain newlines.
  static bool ParsePEMKeyBytes(const std::string& input, std::string* output);

  // Does a simple base64 encoding of |input| into |output|.
  static bool ProducePEM(const std::string& input, std::string* output);

  // Note: The result is coverted to lower-case because the browser enforces
  // hosts to be lower-case in omni-bar.
  static bool GenerateIdFromPublicKey(const std::string& input,
                                      std::string* output);

  // Expects base64 encoded |input| and formats into |output| including
  // the appropriate header & footer.
  static bool FormatPEMForFileOutput(const std::string input,
      std::string* output, bool is_public);

  // Initialize the extension from a parsed manifest.
  // If |require_id| is true, will return an error if the "id" key is missing
  // from the value.
  bool InitFromValue(const DictionaryValue& value, bool require_id,
                     std::string* error);

  const FilePath& path() const { return path_; }
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
  const UserScriptList& content_scripts() const { return content_scripts_; }
  const PageActionMap& page_actions() const { return page_actions_; }
  const std::vector<PluginInfo>& plugins() const { return plugins_; }
  const GURL& background_url() const { return background_url_; }
  const std::vector<std::string>& toolstrips() const { return toolstrips_; }
  const std::vector<URLPattern>& permissions() const { return permissions_; }
  const GURL& update_url() const { return update_url_; }
  const std::map<int, std::string>& icons() { return icons_; }

  // Retrieves a page action by |id|.
  const PageAction* GetPageAction(std::string id) const;

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
  bool IsTheme() { return is_theme_; }

  // Returns a list of paths (relative to the extension dir) for images that
  // the browser might load (like themes and page action icons).
  std::set<FilePath> GetBrowserImages();

 private:
  // Counter used to assign ids to extensions that are loaded using
  // --load-extension.
  static int id_counter_;

  // Returns the next counter id. Intentionally post-incrementing so that first
  // value is 0.
  static int NextGeneratedId() { return id_counter_++; }

  // Helper method that loads a UserScript object from a
  // dictionary in the content_script list of the manifest.
  bool LoadUserScriptHelper(const DictionaryValue* content_script,
                            int definition_index,
                            std::string* error,
                            UserScript* result);

  // Helper method that loads a PageAction object from a
  // dictionary in the page_action list of the manifest.
  PageAction* LoadPageActionHelper(const DictionaryValue* page_action,
                                   int definition_index,
                                   std::string* error);

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

  // Paths to the content scripts the extension contains.
  UserScriptList content_scripts_;

  // A list of page actions.
  PageActionMap page_actions_;

  // Optional list of NPAPI plugins and associated properties.
  std::vector<PluginInfo> plugins_;

  // Optional URL to a master page of which a single instance should be always
  // loaded in the background.
  GURL background_url_;

  // Paths to HTML files to be displayed in the toolbar.
  std::vector<std::string> toolstrips_;

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

  // The sites this extension has permission to talk to (using XHR, etc).
  std::vector<URLPattern> permissions_;

  // The paths to the icons the extension contains mapped by their width.
  std::map<int, std::string> icons_;

  // URL for fetching an update manifest
  GURL update_url_;

  FRIEND_TEST(ExtensionTest, LoadPageActionHelper);

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
