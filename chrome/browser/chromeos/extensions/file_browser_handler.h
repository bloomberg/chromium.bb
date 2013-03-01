// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/gurl.h"

class URLPattern;

// FileBrowserHandler encapsulates the state of a file browser action.
class FileBrowserHandler {
 public:
  typedef std::vector<linked_ptr<FileBrowserHandler> > List;

  // Returns true iff the extension with id |extension_id| is allowed to use
  // MIME type filters.
  static bool ExtensionWhitelistedForMIMETypes(const std::string& extension_id);

  // Returns list of extensions' ids that are allowed to use MIME type filters.
  static std::vector<std::string> GetMIMETypeWhitelist();

  // Whitelists the extension to use MIME type filters for a test.
  // |extension_id| should be owned by the test code.
  static void set_extension_whitelisted_for_test(std::string* extension_id) {
    g_test_extension_id_ = extension_id;
  }

  FileBrowserHandler();
  ~FileBrowserHandler();

  // extension id
  std::string extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  // action id
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // default title
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // File schema URL patterns.
  const extensions::URLPatternSet& file_url_patterns() const {
    return url_set_;
  }
  void AddPattern(const URLPattern& pattern);
  bool MatchesURL(const GURL& url) const;
  void ClearPatterns();

  // Adds a MIME type filter to the handler.
  void AddMIMEType(const std::string& mime_type);
  // Tests if the handler has registered a filter for the MIME type.
  bool CanHandleMIMEType(const std::string& mime_type) const;

  // Action icon path.
  const std::string icon_path() const { return default_icon_path_; }
  void set_icon_path(const std::string& path) {
    default_icon_path_ = path;
  }

  // File access permissions.
  // Adjusts file_access_permission_flags_ to allow specified permission.
  bool AddFileAccessPermission(const std::string& permission_str);
  // Checks that specified file access permissions are valid (all set
  // permissions are valid and there is no other permission specified with
  // "create")
  // If no access permissions were set, initialize them to default value.
  bool ValidateFileAccessPermissions();
  // Checks if handler has read access.
  bool CanRead() const;
  // Checks if handler has write access.
  bool CanWrite() const;
  // Checks if handler has "create" access specified.
  bool HasCreateAccessPermission() const;

  // Returns the file browser handlers associated with the |extension|.
  static List* GetHandlers(const extensions::Extension* extension);

 private:
  // The id of the extension that will be whitelisted to use MIME type filters
  // during tests.
  static std::string* g_test_extension_id_;

  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  std::string extension_id_;
  std::string title_;
  std::string default_icon_path_;
  // The id for the FileBrowserHandler, for example: "PdfFileAction".
  std::string id_;
  unsigned int file_access_permission_flags_;

  // A list of file filters.
  extensions::URLPatternSet url_set_;
  // A list of MIME type filters.
  std::set<std::string> mime_type_set_;
};

// Parses the "file_browser_handlers" extension manifest key.
class FileBrowserHandlerParser : public extensions::ManifestHandler {
 public:
  FileBrowserHandlerParser();
  virtual ~FileBrowserHandlerParser();

  virtual bool Parse(extensions::Extension* extension,
                     string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_H_
