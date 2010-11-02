// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility class to access Chrome Extension manifest data.

#ifndef CEEE_IE_COMMON_EXTENSION_MANIFEST_H_
#define CEEE_IE_COMMON_EXTENSION_MANIFEST_H_

#include <wtypes.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/common/extensions/user_script.h"
#include "googleurl/src/gurl.h"


class DictionaryValue;


// A helper class to read data from a Chrome Extension manifest file.
// TODO(mad@chromium.org): Find a way to reuse code from chrome
// extension classes.
class ExtensionManifest {
 public:
  ExtensionManifest() {}
  ~ExtensionManifest() {}

  // The name of the manifest inside an extension.
  static const char kManifestFilename[];

  // The number of bytes in a legal id.
  static const size_t kIdSize;

  // Clears the current content and reset it with the content of the manifest
  // file found under the provided Chrome extension folder path.
  HRESULT ReadManifestFile(const FilePath& extension_path, bool require_id);

  // Returns the extension ID as read from manifest without any transformation.
  const std::string& extension_id() const {
    return extension_id_;
  }

  // Returns the public key as read from manifest without any transformation.
  const std::string& public_key() const {
    return public_key_;
  }

  // Returns the list of toolstrip file names.
  const std::vector<std::string>& GetToolstripFileNames() const {
    return toolstrip_file_names_;
  }

  const FilePath& path() const { return path_; }
  const GURL& extension_url() const { return extension_url_; }
  const UserScriptList& content_scripts() const { return content_scripts_; }

  // Returns an absolute url to a resource inside of an extension. The
  // |extension_url| argument should be the url() from an Extension object. The
  // |relative_path| can be untrusted user input. The returned URL will either
  // be invalid() or a child of |extension_url|.
  // NOTE: Static so that it can be used from multiple threads.
  static GURL GetResourceUrl(const GURL& extension_url,
                             const std::string& relative_path);
  GURL GetResourceUrl(const std::string& relative_path) {
    return GetResourceUrl(extension_url(), relative_path);
  }

 private:
  // Transforms the value public_key_ to set the value of extension_id_
  // using the same algorithm as Chrome.
  HRESULT CalculateIdFromPublicKey();

  // Helper method that loads a UserScript object from a dictionary in the
  // content_script list of the manifest.
  HRESULT LoadUserScriptHelper(const DictionaryValue* content_script,
                               UserScript* result);

  // The absolute path to the directory the extension is stored in.
  FilePath path_;

  // The base extension url for the extension.
  GURL extension_url_;

  // Paths to the content scripts the extension contains.
  UserScriptList content_scripts_;

  std::vector<std::string> toolstrip_file_names_;
  std::string extension_id_;
  std::string public_key_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManifest);
};

#endif  // CEEE_IE_COMMON_EXTENSION_MANIFEST_H_
