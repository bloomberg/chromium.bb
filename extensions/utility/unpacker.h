// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_UTILITY_UNPACKER_H_
#define EXTENSIONS_UTILITY_UNPACKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "extensions/common/manifest.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class Extension;

// This class unpacks an extension.  It is designed to be used in a sandboxed
// child process.  We parse various bits of the extension, then report back to
// the browser process, who then transcodes the pre-parsed bits and writes them
// back out to disk for later use.
class Unpacker {
 public:
  Unpacker(const base::FilePath& working_dir,
           const base::FilePath& extension_dir,
           const std::string& extension_id,
           Manifest::Location location,
           int creation_flags);
  ~Unpacker();

  // Returns true if the given base::FilePath should be unzipped.
  static bool ShouldExtractFile(bool is_theme, const base::FilePath& file_path);

  // Returns true for manifest.json only.
  static bool IsManifestFile(const base::FilePath& file_path);

  // Parse the manifest.json file inside the extension (not in the header).
  static std::unique_ptr<base::DictionaryValue> ReadManifest(
      const base::FilePath& extension_dir,
      std::string* error);

  // Runs the processing steps for the extension. On success, this returns true
  // and the decoded images will be in a file at
  // |working_dir|/kDecodedImagesFilename and the decoded messages will be in a
  // file at |working_dir|/kDecodedMessageCatalogsFilename.
  bool Run();

  const base::string16& error_message() { return error_message_; }
  base::DictionaryValue* parsed_manifest() { return parsed_manifest_.get(); }
  base::DictionaryValue* parsed_catalogs() { return parsed_catalogs_.get(); }

  std::unique_ptr<base::DictionaryValue> TakeParsedManifest() {
    return std::move(parsed_manifest_);
  }

  std::unique_ptr<base::ListValue> TakeParsedJSONRuleset() {
    return std::move(parsed_json_ruleset_);
  }

 private:
  // Reads the Declarative Net Request API JSON ruleset for |extension| if it
  // provided one. Returns false and populates |error_message_| in case of an
  // error.
  bool ReadJSONRulesetIfNeeded(const Extension* extension);

  // Write the decoded images to kDecodedImagesFilename.  We do this instead
  // of sending them over IPC, since they are so large.  Returns true on
  // success.
  bool DumpImagesToFile();

  // Write the decoded messages to kDecodedMessageCatalogsFilename.  We do this
  // instead of sending them over IPC, since they are so large.  Returns true on
  // success.
  bool DumpMessageCatalogsToFile();

  // Parse all _locales/*/messages.json files inside the extension.
  bool ReadAllMessageCatalogs();

  // Decodes the image at the given path and puts it in our list of decoded
  // images.
  bool AddDecodedImage(const base::FilePath& path);

  // Parses the catalog at the given path and puts it in our list of parsed
  // catalogs.
  bool ReadMessageCatalog(const base::FilePath& message_path);

  // Set the error message.
  void SetError(const std::string& error);
  void SetUTF16Error(const base::string16& error);

  // The directory to do work in.
  base::FilePath working_dir_;

  // The directory where the extension source lives.
  base::FilePath extension_dir_;

  // The extension ID if known.
  std::string extension_id_;

  // The location to use for the created extension.
  Manifest::Location location_;

  // The creation flags to use with the created extension.
  int creation_flags_;

  // The parsed version of the manifest JSON contained in the extension.
  std::unique_ptr<base::DictionaryValue> parsed_manifest_;

  // The parsed version of the Declarative Net Request API ruleset. Null if the
  // extension did not provide one.
  std::unique_ptr<base::ListValue> parsed_json_ruleset_;

  // A list of decoded images and the paths where those images came from.  Paths
  // are relative to the manifest file.
  struct InternalData;
  std::unique_ptr<InternalData> internal_data_;

  // Dictionary of relative paths and catalogs per path. Paths are in the form
  // of _locales/locale, without messages.json base part.
  std::unique_ptr<base::DictionaryValue> parsed_catalogs_;

  // The last error message that was set.  Empty if there were no errors.
  base::string16 error_message_;

  DISALLOW_COPY_AND_ASSIGN(Unpacker);
};

}  // namespace extensions

#endif  // EXTENSIONS_UTILITY_UNPACKER_H_
