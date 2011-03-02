// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_unpacker.h"

#include <set>

#include "base/file_util.h"
#include "base/scoped_handle.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/file_stream.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/zip.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;
namespace filenames = extension_filenames;

namespace {

// Errors
const char* kCouldNotCreateDirectoryError =
    "Could not create directory for unzipping: ";
const char* kCouldNotDecodeImageError = "Could not decode theme image.";
const char* kCouldNotUnzipExtension = "Could not unzip extension.";
const char* kPathNamesMustBeAbsoluteOrLocalError =
    "Path names must not be absolute or contain '..'.";

// A limit to stop us passing dangerously large canvases to the browser.
const int kMaxImageCanvas = 4096 * 4096;

SkBitmap DecodeImage(const FilePath& path) {
  // Read the file from disk.
  std::string file_contents;
  if (!file_util::PathExists(path) ||
      !file_util::ReadFileToString(path, &file_contents)) {
    return SkBitmap();
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());
  webkit_glue::ImageDecoder decoder;
  SkBitmap bitmap = decoder.Decode(data, file_contents.length());
  Sk64 bitmap_size = bitmap.getSize64();
  if (!bitmap_size.is32() || bitmap_size.get32() > kMaxImageCanvas)
    return SkBitmap();
  return bitmap;
}

bool PathContainsParentDirectory(const FilePath& path) {
  const FilePath::StringType kSeparators(FilePath::kSeparators);
  const FilePath::StringType kParentDirectory(FilePath::kParentDirectory);
  const size_t npos = FilePath::StringType::npos;
  const FilePath::StringType& value = path.value();

  for (size_t i = 0; i < value.length(); ) {
    i = value.find(kParentDirectory, i);
    if (i != npos) {
      if ((i == 0 || kSeparators.find(value[i-1]) == npos) &&
          (i+1 < value.length() || kSeparators.find(value[i+1]) == npos)) {
        return true;
      }
      ++i;
    }
  }

  return false;
}

}  // namespace

ExtensionUnpacker::ExtensionUnpacker(const FilePath& extension_path)
    : extension_path_(extension_path) {
}

ExtensionUnpacker::~ExtensionUnpacker() {
}

DictionaryValue* ExtensionUnpacker::ReadManifest() {
  FilePath manifest_path =
      temp_install_dir_.Append(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    SetError(errors::kInvalidManifest);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(NULL, &error));
  if (!root.get()) {
    SetError(error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    SetError(errors::kInvalidManifest);
    return NULL;
  }

  return static_cast<DictionaryValue*>(root.release());
}

bool ExtensionUnpacker::ReadAllMessageCatalogs(
    const std::string& default_locale) {
  FilePath locales_path =
    temp_install_dir_.Append(Extension::kLocaleFolder);

  // Not all folders under _locales have to be valid locales.
  file_util::FileEnumerator locales(locales_path,
                                    false,
                                    file_util::FileEnumerator::DIRECTORIES);

  std::set<std::string> all_locales;
  extension_l10n_util::GetAllLocales(&all_locales);
  FilePath locale_path;
  while (!(locale_path = locales.Next()).empty()) {
    if (extension_l10n_util::ShouldSkipValidation(locales_path, locale_path,
                                                  all_locales))
      continue;

    FilePath messages_path =
      locale_path.Append(Extension::kMessagesFilename);

    if (!ReadMessageCatalog(messages_path))
      return false;
  }

  return true;
}

bool ExtensionUnpacker::Run() {
  VLOG(1) << "Installing extension " << extension_path_.value();

  // <profile>/Extensions/INSTALL_TEMP/<version>
  temp_install_dir_ =
    extension_path_.DirName().AppendASCII(filenames::kTempExtensionName);

  if (!file_util::CreateDirectory(temp_install_dir_)) {
#if defined(OS_WIN)
    std::string dir_string = WideToUTF8(temp_install_dir_.value());
#else
    std::string dir_string = temp_install_dir_.value();
#endif

    SetError(kCouldNotCreateDirectoryError + dir_string);
    return false;
  }

  if (!Unzip(extension_path_, temp_install_dir_)) {
    SetError(kCouldNotUnzipExtension);
    return false;
  }

  // Parse the manifest.
  parsed_manifest_.reset(ReadManifest());
  if (!parsed_manifest_.get())
    return false;  // Error was already reported.

  // NOTE: Since the unpacker doesn't have the extension's public_id, the
  // InitFromValue is allowed to generate a temporary id for the extension.
  // ANY CODE THAT FOLLOWS SHOULD NOT DEPEND ON THE CORRECT ID OF THIS
  // EXTENSION.
  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      temp_install_dir_, Extension::INVALID, *parsed_manifest_, false, &error));
  if (!extension.get()) {
    SetError(error);
    return false;
  }

  if (!extension_file_util::ValidateExtension(extension.get(), &error)) {
    SetError(error);
    return false;
  }

  // Decode any images that the browser needs to display.
  std::set<FilePath> image_paths = extension->GetBrowserImages();
  for (std::set<FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    if (!AddDecodedImage(*it))
      return false;  // Error was already reported.
  }

  // Parse all message catalogs (if any).
  parsed_catalogs_.reset(new DictionaryValue);
  if (!extension->default_locale().empty()) {
    if (!ReadAllMessageCatalogs(extension->default_locale()))
      return false;  // Error was already reported.
  }

  return true;
}

bool ExtensionUnpacker::DumpImagesToFile() {
  IPC::Message pickle;  // We use a Message so we can use WriteParam.
  IPC::WriteParam(&pickle, decoded_images_);

  FilePath path = extension_path_.DirName().AppendASCII(
      filenames::kDecodedImagesFilename);
  if (!file_util::WriteFile(path, static_cast<const char*>(pickle.data()),
                            pickle.size())) {
    SetError("Could not write image data to disk.");
    return false;
  }

  return true;
}

bool ExtensionUnpacker::DumpMessageCatalogsToFile() {
  IPC::Message pickle;
  IPC::WriteParam(&pickle, *parsed_catalogs_.get());

  FilePath path = extension_path_.DirName().AppendASCII(
      filenames::kDecodedMessageCatalogsFilename);
  if (!file_util::WriteFile(path, static_cast<const char*>(pickle.data()),
                            pickle.size())) {
    SetError("Could not write message catalogs to disk.");
    return false;
  }

  return true;
}

// static
bool ExtensionUnpacker::ReadImagesFromFile(const FilePath& extension_path,
                                           DecodedImages* images) {
  FilePath path = extension_path.AppendASCII(filenames::kDecodedImagesFilename);
  std::string file_str;
  if (!file_util::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  void* iter = NULL;
  return IPC::ReadParam(&pickle, &iter, images);
}

// static
bool ExtensionUnpacker::ReadMessageCatalogsFromFile(
    const FilePath& extension_path, DictionaryValue* catalogs) {
  FilePath path = extension_path.AppendASCII(
      filenames::kDecodedMessageCatalogsFilename);
  std::string file_str;
  if (!file_util::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  void* iter = NULL;
  return IPC::ReadParam(&pickle, &iter, catalogs);
}

bool ExtensionUnpacker::AddDecodedImage(const FilePath& path) {
  // Make sure it's not referencing a file outside the extension's subdir.
  if (path.IsAbsolute() || PathContainsParentDirectory(path)) {
    SetError(kPathNamesMustBeAbsoluteOrLocalError);
    return false;
  }

  SkBitmap image_bitmap = DecodeImage(temp_install_dir_.Append(path));
  if (image_bitmap.isNull()) {
    SetError(kCouldNotDecodeImageError);
    return false;
  }

  decoded_images_.push_back(MakeTuple(image_bitmap, path));
  return true;
}

bool ExtensionUnpacker::ReadMessageCatalog(const FilePath& message_path) {
  std::string error;
  JSONFileValueSerializer serializer(message_path);
  scoped_ptr<DictionaryValue> root(
      static_cast<DictionaryValue*>(serializer.Deserialize(NULL, &error)));
  if (!root.get()) {
    string16 messages_file = message_path.LossyDisplayName();
    if (error.empty()) {
      // If file is missing, Deserialize will fail with empty error.
      SetError(base::StringPrintf("%s %s", errors::kLocalesMessagesFileMissing,
                                  UTF16ToUTF8(messages_file).c_str()));
    } else {
      SetError(base::StringPrintf("%s: %s",
                                  UTF16ToUTF8(messages_file).c_str(),
                                  error.c_str()));
    }
    return false;
  }

  FilePath relative_path;
  // message_path was created from temp_install_dir. This should never fail.
  if (!temp_install_dir_.AppendRelativePath(message_path, &relative_path)) {
    NOTREACHED();
    return false;
  }

  std::string dir_name = relative_path.DirName().MaybeAsASCII();
  if (dir_name.empty()) {
    NOTREACHED();
    return false;
  }
  parsed_catalogs_->Set(dir_name, root.release());

  return true;
}

void ExtensionUnpacker::SetError(const std::string &error) {
  error_message_ = error;
}
