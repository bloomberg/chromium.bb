// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_UNPACKER_H_
#define CHROME_COMMON_EXTENSIONS_UNPACKER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/tuple.h"
#include "chrome/common/extensions/manifest.h"

class SkBitmap;

namespace base {
class DictionaryValue;
}

namespace extensions {

// This class unpacks an extension.  It is designed to be used in a sandboxed
// child process.  We unpack and parse various bits of the extension, then
// report back to the browser process, who then transcodes the pre-parsed bits
// and writes them back out to disk for later use.
class Unpacker {
 public:
  typedef std::vector< Tuple2<SkBitmap, base::FilePath> > DecodedImages;

  Unpacker(const base::FilePath& extension_path,
           const std::string& extension_id,
           Manifest::Location location,
           int creation_flags);
  ~Unpacker();

  // Install the extension file at |extension_path|.  Returns true on success.
  // Otherwise, error_message will contain a string explaining what went wrong.
  bool Run();

  // Write the decoded images to kDecodedImagesFilename.  We do this instead
  // of sending them over IPC, since they are so large.  Returns true on
  // success.
  bool DumpImagesToFile();

  // Write the decoded messages to kDecodedMessageCatalogsFilename.  We do this
  // instead of sending them over IPC, since they are so large.  Returns true on
  // success.
  bool DumpMessageCatalogsToFile();

  // Read the decoded images back from the file we saved them to.
  // |extension_path| is the path to the extension we unpacked that wrote the
  // data. Returns true on success.
  static bool ReadImagesFromFile(const base::FilePath& extension_path,
                                 DecodedImages* images);

  // Read the decoded message catalogs back from the file we saved them to.
  // |extension_path| is the path to the extension we unpacked that wrote the
  // data. Returns true on success.
  static bool ReadMessageCatalogsFromFile(const base::FilePath& extension_path,
                                          base::DictionaryValue* catalogs);

  const string16& error_message() { return error_message_; }
  base::DictionaryValue* parsed_manifest() {
    return parsed_manifest_.get();
  }
  const DecodedImages& decoded_images() { return decoded_images_; }
  base::DictionaryValue* parsed_catalogs() { return parsed_catalogs_.get(); }

 private:
  // Parse the manifest.json file inside the extension (not in the header).
  // Caller takes ownership of return value.
  base::DictionaryValue* ReadManifest();

  // Parse all _locales/*/messages.json files inside the extension.
  bool ReadAllMessageCatalogs(const std::string& default_locale);

  // Decodes the image at the given path and puts it in our list of decoded
  // images.
  bool AddDecodedImage(const base::FilePath& path);

  // Parses the catalog at the given path and puts it in our list of parsed
  // catalogs.
  bool ReadMessageCatalog(const base::FilePath& message_path);

  // Set the error message.
  void SetError(const std::string& error);
  void SetUTF16Error(const string16& error);

  // The extension to unpack.
  base::FilePath extension_path_;

  // The extension ID if known.
  std::string extension_id_;

  // The location to use for the created extension.
  Manifest::Location location_;

  // The creation flags to use with the created extension.
  int creation_flags_;

  // The place we unpacked the extension to.
  base::FilePath temp_install_dir_;

  // The parsed version of the manifest JSON contained in the extension.
  scoped_ptr<base::DictionaryValue> parsed_manifest_;

  // A list of decoded images and the paths where those images came from.  Paths
  // are relative to the manifest file.
  DecodedImages decoded_images_;

  // Dictionary of relative paths and catalogs per path. Paths are in the form
  // of _locales/locale, without messages.json base part.
  scoped_ptr<base::DictionaryValue> parsed_catalogs_;

  // The last error message that was set.  Empty if there were no errors.
  string16 error_message_;

  DISALLOW_COPY_AND_ASSIGN(Unpacker);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_UNPACKER_H_
