// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"

#include <set>

#include "base/base64.h"
#include "base/crypto/signature_verifier.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/scoped_handle.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"  // TODO(viettrungluu): delete me.
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "chrome/common/json_value_serializer.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

const char SandboxedExtensionUnpacker::kExtensionHeaderMagic[] = "Cr24";

SandboxedExtensionUnpacker::SandboxedExtensionUnpacker(
    const FilePath& crx_path,
    ResourceDispatcherHost* rdh,
    SandboxedExtensionUnpackerClient* client)
    : crx_path_(crx_path),
      thread_identifier_(BrowserThread::ID_COUNT),
      rdh_(rdh), client_(client), got_response_(false) {
}

bool SandboxedExtensionUnpacker::CreateTempDirectory() {
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_identifier_));

  FilePath user_data_temp_dir = extension_file_util::GetUserDataTempDir();
  if (user_data_temp_dir.empty()) {
    ReportFailure(
        COULD_NOT_GET_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_GET_TEMP_DIRECTORY")));
    return false;
  }

  if (!temp_dir_.CreateUniqueTempDirUnderPath(user_data_temp_dir)) {
    ReportFailure(
        COULD_NOT_CREATE_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_CREATE_TEMP_DIRECTORY")));
    return false;
  }

  return true;
}

void SandboxedExtensionUnpacker::Start() {
  // We assume that we are started on the thread that the client wants us to do
  // file IO on.
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_identifier_));

  if (!CreateTempDirectory())
    return;  // ReportFailure() already called.

  // Initialize the path that will eventually contain the unpacked extension.
  extension_root_ = temp_dir_.path().AppendASCII(
      extension_filenames::kTempExtensionName);

  // Extract the public key and validate the package.
  if (!ValidateSignature())
    return;  // ValidateSignature() already reported the error.

  // Copy the crx file into our working directory.
  FilePath temp_crx_path = temp_dir_.path().Append(crx_path_.BaseName());
  if (!file_util::CopyFile(crx_path_, temp_crx_path)) {
    // Failed to copy extension file to temporary directory.
    ReportFailure(
        FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY")));
    return;
  }

  // If we are supposed to use a subprocess, kick off the subprocess.
  //
  // TODO(asargent) we shouldn't need to do this branch here - instead
  // UtilityProcessHost should handle it for us. (http://crbug.com/19192)
  bool use_utility_process = rdh_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
  if (use_utility_process) {
    // The utility process will have access to the directory passed to
    // SandboxedExtensionUnpacker.  That directory should not contain a
    // symlink or NTFS reparse point.  When the path is used, following
    // the link/reparse point will cause file system access outside the
    // sandbox path, and the sandbox will deny the operation.
    FilePath link_free_crx_path;
    if (!file_util::NormalizeFilePath(temp_crx_path, &link_free_crx_path)) {
      LOG(ERROR) << "Could not get the normalized path of "
                 << temp_crx_path.value();
      ReportFailure(
          COULD_NOT_GET_SANDBOX_FRIENDLY_PATH,
          l10n_util::GetStringUTF8(IDS_EXTENSION_UNPACK_FAILED));
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &SandboxedExtensionUnpacker::StartProcessOnIOThread,
            link_free_crx_path));
  } else {
    // Otherwise, unpack the extension in this process.
    ExtensionUnpacker unpacker(temp_crx_path);
    if (unpacker.Run() && unpacker.DumpImagesToFile() &&
        unpacker.DumpMessageCatalogsToFile()) {
      OnUnpackExtensionSucceeded(*unpacker.parsed_manifest());
    } else {
      OnUnpackExtensionFailed(unpacker.error_message());
    }
  }
}

SandboxedExtensionUnpacker::~SandboxedExtensionUnpacker() {
  base::FileUtilProxy::Delete(
      BrowserThread::GetMessageLoopProxyForThread(thread_identifier_),
      temp_dir_.Take(),
      true,
      NULL);
}

void SandboxedExtensionUnpacker::StartProcessOnIOThread(
    const FilePath& temp_crx_path) {
  UtilityProcessHost* host = new UtilityProcessHost(
      rdh_, this, thread_identifier_);
  host->StartExtensionUnpacker(temp_crx_path);
}

void SandboxedExtensionUnpacker::OnUnpackExtensionSucceeded(
    const DictionaryValue& manifest) {
  // Skip check for unittests.
  if (thread_identifier_ != BrowserThread::ID_COUNT)
    CHECK(BrowserThread::CurrentlyOn(thread_identifier_));
  got_response_ = true;

  scoped_ptr<DictionaryValue> final_manifest(RewriteManifestFile(manifest));
  if (!final_manifest.get())
    return;

  // Create an extension object that refers to the temporary location the
  // extension was unpacked to. We use this until the extension is finally
  // installed. For example, the install UI shows images from inside the
  // extension.

  // Localize manifest now, so confirm UI gets correct extension name.
  std::string error;
  if (!extension_l10n_util::LocalizeExtension(extension_root_,
                                              final_manifest.get(),
                                              &error)) {
    ReportFailure(
        COULD_NOT_LOCALIZE_EXTENSION,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
            ASCIIToUTF16(error)));
    return;
  }

  extension_ = Extension::Create(
      extension_root_, Extension::INTERNAL, *final_manifest, true, &error);

  if (!extension_.get()) {
    ReportFailure(
        INVALID_MANIFEST,
        std::string("Manifest is invalid: ") + error);
    return;
  }

  if (!RewriteImageFiles())
    return;

  if (!RewriteCatalogFiles())
    return;

  ReportSuccess();
}

void SandboxedExtensionUnpacker::OnUnpackExtensionFailed(
    const std::string& error) {
  CHECK(BrowserThread::CurrentlyOn(thread_identifier_));
  got_response_ = true;
  ReportFailure(
      UNPACKER_CLIENT_FAILED,
      l10n_util::GetStringFUTF8(
           IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
           ASCIIToUTF16(error)));
}

void SandboxedExtensionUnpacker::OnProcessCrashed(int exit_code) {
  // Don't report crashes if they happen after we got a response.
  if (got_response_)
    return;

  // Utility process crashed while trying to install.
  ReportFailure(
     UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,
     l10n_util::GetStringFUTF8(
         IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
         ASCIIToUTF16("UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL")));
}

bool SandboxedExtensionUnpacker::ValidateSignature() {
  ScopedStdioHandle file(file_util::OpenFile(crx_path_, "rb"));
  if (!file.get()) {
    // Could not open crx file for reading
    ReportFailure(
        CRX_FILE_NOT_READABLE,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_FILE_NOT_READABLE")));
    return false;
  }

  // Read and verify the header.
  ExtensionHeader header;
  size_t len;

  // TODO(erikkay): Yuck.  I'm not a big fan of this kind of code, but it
  // appears that we don't have any endian/alignment aware serialization
  // code in the code base.  So for now, this assumes that we're running
  // on a little endian machine with 4 byte alignment.
  len = fread(&header, 1, sizeof(ExtensionHeader),
      file.get());
  if (len < sizeof(ExtensionHeader)) {
    // Invalid crx header
    ReportFailure(
        CRX_HEADER_INVALID,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_HEADER_INVALID")));
    return false;
  }
  if (strncmp(kExtensionHeaderMagic, header.magic,
      sizeof(header.magic))) {
    // Bad magic number
    ReportFailure(
        CRX_MAGIC_NUMBER_INVALID,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_MAGIC_NUMBER_INVALID")));
    return false;
  }
  if (header.version != kCurrentVersion) {
    // Bad version numer
    ReportFailure(CRX_VERSION_NUMBER_INVALID,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_VERSION_NUMBER_INVALID")));
    return false;
  }
  if (header.key_size > kMaxPublicKeySize ||
      header.signature_size > kMaxSignatureSize) {
    // Excessively large key or signature
    ReportFailure(
        CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE")));
    return false;
  }
  if (header.key_size == 0) {
    // Key length is zero
    ReportFailure(
        CRX_ZERO_KEY_LENGTH,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_ZERO_KEY_LENGTH")));
    return false;
  }
  if (header.signature_size == 0) {
    // Signature length is zero
    ReportFailure(
        CRX_ZERO_SIGNATURE_LENGTH,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_ZERO_SIGNATURE_LENGTH")));
    return false;
  }

  std::vector<uint8> key;
  key.resize(header.key_size);
  len = fread(&key.front(), sizeof(uint8), header.key_size, file.get());
  if (len < header.key_size) {
    // Invalid public key
    ReportFailure(
        CRX_PUBLIC_KEY_INVALID,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_PUBLIC_KEY_INVALID")));
    return false;
  }

  std::vector<uint8> signature;
  signature.resize(header.signature_size);
  len = fread(&signature.front(), sizeof(uint8), header.signature_size,
      file.get());
  if (len < header.signature_size) {
    // Invalid signature
    ReportFailure(
        CRX_SIGNATURE_INVALID,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_SIGNATURE_INVALID")));
    return false;
  }

  base::SignatureVerifier verifier;
  if (!verifier.VerifyInit(extension_misc::kSignatureAlgorithm,
                           sizeof(extension_misc::kSignatureAlgorithm),
                           &signature.front(),
                           signature.size(),
                           &key.front(),
                           key.size())) {
    // Signature verification initialization failed. This is most likely
    // caused by a public key in the wrong format (should encode algorithm).
    ReportFailure(
        CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED")));
    return false;
  }

  unsigned char buf[1 << 12];
  while ((len = fread(buf, 1, sizeof(buf), file.get())) > 0)
    verifier.VerifyUpdate(buf, len);

  if (!verifier.VerifyFinal()) {
    // Signature verification failed
    ReportFailure(
        CRX_SIGNATURE_VERIFICATION_FAILED,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_SIGNATURE_VERIFICATION_FAILED")));
    return false;
  }

  base::Base64Encode(std::string(reinterpret_cast<char*>(&key.front()),
      key.size()), &public_key_);
  return true;
}

void SandboxedExtensionUnpacker::ReportFailure(FailureReason reason,
                                               const std::string& error) {
  UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackFailure", 1);
  UMA_HISTOGRAM_ENUMERATION("Extensions.SandboxUnpackFailureReason",
                            reason, NUM_FAILURE_REASONS);

  client_->OnUnpackFailure(error);
}

void SandboxedExtensionUnpacker::ReportSuccess() {
  UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackSuccess", 1);

  // Client takes ownership of temporary directory and extension.
  client_->OnUnpackSuccess(temp_dir_.Take(), extension_root_, extension_);
  extension_ = NULL;
}

DictionaryValue* SandboxedExtensionUnpacker::RewriteManifestFile(
    const DictionaryValue& manifest) {
  // Add the public key extracted earlier to the parsed manifest and overwrite
  // the original manifest. We do this to ensure the manifest doesn't contain an
  // exploitable bug that could be used to compromise the browser.
  scoped_ptr<DictionaryValue> final_manifest(manifest.DeepCopy());
  final_manifest->SetString(extension_manifest_keys::kPublicKey, public_key_);

  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(*final_manifest)) {
    // Error serializing manifest.json.
    ReportFailure(
        ERROR_SERIALIZING_MANIFEST_JSON,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("ERROR_SERIALIZING_MANIFEST_JSON")));
    return NULL;
  }

  FilePath manifest_path =
      extension_root_.Append(Extension::kManifestFilename);
  if (!file_util::WriteFile(manifest_path,
                            manifest_json.data(), manifest_json.size())) {
    // Error saving manifest.json.
    ReportFailure(
        ERROR_SAVING_MANIFEST_JSON,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("ERROR_SAVING_MANIFEST_JSON")));
    return NULL;
  }

  return final_manifest.release();
}

bool SandboxedExtensionUnpacker::RewriteImageFiles() {
  ExtensionUnpacker::DecodedImages images;
  if (!ExtensionUnpacker::ReadImagesFromFile(temp_dir_.path(), &images)) {
    // Couldn't read image data from disk.
    ReportFailure(
        COULD_NOT_READ_IMAGE_DATA_FROM_DISK,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_READ_IMAGE_DATA_FROM_DISK")));
    return false;
  }

  // Delete any images that may be used by the browser.  We're going to write
  // out our own versions of the parsed images, and we want to make sure the
  // originals are gone for good.
  std::set<FilePath> image_paths = extension_->GetBrowserImages();
  if (image_paths.size() != images.size()) {
    // Decoded images don't match what's in the manifest.
    ReportFailure(
        DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST")));
    return false;
  }

  for (std::set<FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    FilePath path = *it;
    if (path.IsAbsolute() || path.ReferencesParent()) {
      // Invalid path for browser image.
      ReportFailure(
          INVALID_PATH_FOR_BROWSER_IMAGE,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_PATH_FOR_BROWSER_IMAGE")));
      return false;
    }
    if (!file_util::Delete(extension_root_.Append(path), false)) {
      // Error removing old image file.
      ReportFailure(
          ERROR_REMOVING_OLD_IMAGE_FILE,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_REMOVING_OLD_IMAGE_FILE")));
      return false;
    }
  }

  // Write our parsed images back to disk as well.
  for (size_t i = 0; i < images.size(); ++i) {
    const SkBitmap& image = images[i].a;
    FilePath path_suffix = images[i].b;
    if (path_suffix.IsAbsolute() || path_suffix.ReferencesParent()) {
      // Invalid path for bitmap image.
      ReportFailure(
          INVALID_PATH_FOR_BITMAP_IMAGE,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_PATH_FOR_BITMAP_IMAGE")));
      return false;
    }
    FilePath path = extension_root_.Append(path_suffix);

    std::vector<unsigned char> image_data;
    // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
    // though they may originally be .jpg, etc.  Figure something out.
    // http://code.google.com/p/chromium/issues/detail?id=12459
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data)) {
      // Error re-encoding theme image.
      ReportFailure(
          ERROR_RE_ENCODING_THEME_IMAGE,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_RE_ENCODING_THEME_IMAGE")));
      return false;
    }

    // Note: we're overwriting existing files that the utility process wrote,
    // so we can be sure the directory exists.
    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    if (!file_util::WriteFile(path, image_data_ptr, image_data.size())) {
      // Error saving theme image.
      ReportFailure(
          ERROR_SAVING_THEME_IMAGE,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_SAVING_THEME_IMAGE")));
      return false;
    }
  }

  return true;
}

bool SandboxedExtensionUnpacker::RewriteCatalogFiles() {
  DictionaryValue catalogs;
  if (!ExtensionUnpacker::ReadMessageCatalogsFromFile(temp_dir_.path(),
                                                      &catalogs)) {
    // Could not read catalog data from disk.
    ReportFailure(
        COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
        l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_READ_CATALOG_DATA_FROM_DISK")));
    return false;
  }

  // Write our parsed catalogs back to disk.
  for (DictionaryValue::key_iterator key_it = catalogs.begin_keys();
       key_it != catalogs.end_keys(); ++key_it) {
    DictionaryValue* catalog;
    if (!catalogs.GetDictionaryWithoutPathExpansion(*key_it, &catalog)) {
      // Invalid catalog data.
      ReportFailure(
          INVALID_CATALOG_DATA,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_CATALOG_DATA")));
      return false;
    }

    // TODO(viettrungluu): Fix the |FilePath::FromWStringHack(UTF8ToWide())|
    // hack and remove the corresponding #include.
    FilePath relative_path = FilePath::FromWStringHack(UTF8ToWide(*key_it));
    relative_path = relative_path.Append(Extension::kMessagesFilename);
    if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
      // Invalid path for catalog.
      ReportFailure(
          INVALID_PATH_FOR_CATALOG,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_PATH_FOR_CATALOG")));
      return false;
    }
    FilePath path = extension_root_.Append(relative_path);

    std::string catalog_json;
    JSONStringValueSerializer serializer(&catalog_json);
    serializer.set_pretty_print(true);
    if (!serializer.Serialize(*catalog)) {
      // Error serializing catalog.
      ReportFailure(
          ERROR_SERIALIZING_CATALOG,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_SERIALIZING_CATALOG")));
      return false;
    }

    // Note: we're overwriting existing files that the utility process read,
    // so we can be sure the directory exists.
    if (!file_util::WriteFile(path,
                              catalog_json.c_str(),
                              catalog_json.size())) {
      // Error saving catalog.
      ReportFailure(
          ERROR_SAVING_CATALOG,
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_SAVING_CATALOG")));
      return false;
    }
  }

  return true;
}
