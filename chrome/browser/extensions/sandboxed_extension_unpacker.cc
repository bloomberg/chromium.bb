// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"

#include <set>

#include "app/gfx/codec/png_codec.h"
#include "base/base64.h"
#include "base/crypto/signature_verifier.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_handle.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "chrome/common/json_value_serializer.h"
#include "third_party/skia/include/core/SkBitmap.h"

const char SandboxedExtensionUnpacker::kExtensionHeaderMagic[] = "Cr24";

SandboxedExtensionUnpacker::SandboxedExtensionUnpacker(
    const FilePath& crx_path, ResourceDispatcherHost* rdh,
    SandboxedExtensionUnpackerClient* client)
      : crx_path_(crx_path), thread_identifier_(ChromeThread::ID_COUNT),
        rdh_(rdh), client_(client), got_response_(false) {
}

void SandboxedExtensionUnpacker::Start() {
  // We assume that we are started on the thread that the client wants us to do
  // file IO on.
  CHECK(ChromeThread::GetCurrentThreadIdentifier(&thread_identifier_));

  // Create a temporary directory to work in.
  if (!temp_dir_.CreateUniqueTempDir()) {
    ReportFailure("Could not create temporary directory.");
    return;
  }

  // Initialize the path that will eventually contain the unpacked extension.
  extension_root_ = temp_dir_.path().AppendASCII("TEMP_INSTALL");

  // Extract the public key and validate the package.
  if (!ValidateSignature())
    return;  // ValidateSignature() already reported the error.

  // Copy the crx file into our working directory.
  FilePath temp_crx_path = temp_dir_.path().Append(crx_path_.BaseName());
  if (!file_util::CopyFile(crx_path_, temp_crx_path)) {
    ReportFailure("Failed to copy extension file to temporary directory.");
    return;
  }

  // If we are supposed to use a subprocess, copy the crx to the temp directory
  // and kick off the subprocess.
  //
  // TODO(asargent) we shouldn't need to do this branch here - instead
  // UtilityProcessHost should handle it for us. (http://crbug.com/19192)
  bool use_utility_process = rdh_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
  if (use_utility_process) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &SandboxedExtensionUnpacker::StartProcessOnIOThread,
            temp_crx_path));
  } else {
    // Otherwise, unpack the extension in this process.
    ExtensionUnpacker unpacker(temp_crx_path);
    if (unpacker.Run() && unpacker.DumpImagesToFile())
      OnUnpackExtensionSucceeded(*unpacker.parsed_manifest(),
                                 *unpacker.parsed_catalogs());
    else
      OnUnpackExtensionFailed(unpacker.error_message());
  }
}

void SandboxedExtensionUnpacker::StartProcessOnIOThread(
    const FilePath& temp_crx_path) {
  UtilityProcessHost* host = new UtilityProcessHost(
      rdh_, this, thread_identifier_);
  host->StartExtensionUnpacker(temp_crx_path);
}

void SandboxedExtensionUnpacker::OnUnpackExtensionSucceeded(
    const DictionaryValue& manifest,
    const DictionaryValue& catalogs) {
  // Skip check for unittests.
  if (thread_identifier_ != ChromeThread::ID_COUNT)
    DCHECK(ChromeThread::CurrentlyOn(thread_identifier_));
  got_response_ = true;

  scoped_ptr<DictionaryValue> final_manifest(RewriteManifestFile(manifest));
  if (!final_manifest.get())
    return;

  // Create an extension object that refers to the temporary location the
  // extension was unpacked to. We use this until the extension is finally
  // installed. For example, the install UI shows images from inside the
  // extension.
  extension_.reset(new Extension(extension_root_));

  std::string manifest_error;
  if (!extension_->InitFromValue(*final_manifest, true,  // require id
                                 &manifest_error)) {
    ReportFailure(std::string("Manifest is invalid: ") +
                              manifest_error);
    return;
  }

  if (!RewriteImageFiles())
    return;

  if (!RewriteCatalogFiles(catalogs))
    return;

  ReportSuccess();
}

void SandboxedExtensionUnpacker::OnUnpackExtensionFailed(
    const std::string& error) {
  DCHECK(ChromeThread::CurrentlyOn(thread_identifier_));
  got_response_ = true;
  ReportFailure(error);
}

void SandboxedExtensionUnpacker::OnProcessCrashed() {
  // Don't report crashes if they happen after we got a response.
  if (got_response_)
    return;

  ReportFailure("Utility process crashed while trying to install.");
}

bool SandboxedExtensionUnpacker::ValidateSignature() {
  ScopedStdioHandle file(file_util::OpenFile(crx_path_, "rb"));
  if (!file.get()) {
    ReportFailure("Could not open crx file for reading");
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
    ReportFailure("Invalid crx header");
    return false;
  }
  if (strncmp(kExtensionHeaderMagic, header.magic,
      sizeof(header.magic))) {
    ReportFailure("Bad magic number");
    return false;
  }
  if (header.version != kCurrentVersion) {
    ReportFailure("Bad version number");
    return false;
  }
  if (header.key_size > kMaxPublicKeySize ||
      header.signature_size > kMaxSignatureSize) {
    ReportFailure("Excessively large key or signature");
    return false;
  }

  std::vector<uint8> key;
  key.resize(header.key_size);
  len = fread(&key.front(), sizeof(uint8), header.key_size, file.get());
  if (len < header.key_size) {
    ReportFailure("Invalid public key");
    return false;
  }

  std::vector<uint8> signature;
  signature.resize(header.signature_size);
  len = fread(&signature.front(), sizeof(uint8), header.signature_size,
      file.get());
  if (len < header.signature_size) {
    ReportFailure("Invalid signature");
    return false;
  }

  // Note: this structure is an ASN.1 which encodes the algorithm used
  // with its parameters. This is defined in PKCS #1 v2.1 (RFC 3447).
  // It is encoding: { OID sha1WithRSAEncryption      PARAMETERS NULL }
  // TODO(aa): This needs to be factored away someplace common.
  const uint8 signature_algorithm[15] = {
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
  };

  base::SignatureVerifier verifier;
  if (!verifier.VerifyInit(signature_algorithm,
                           sizeof(signature_algorithm),
                           &signature.front(),
                           signature.size(),
                           &key.front(),
                           key.size())) {
    ReportFailure("Signature verification initialization failed. "
                  "This is most likely caused by a public key in "
                  "the wrong format (should encode algorithm).");
    return false;
  }

  unsigned char buf[1 << 12];
  while ((len = fread(buf, 1, sizeof(buf), file.get())) > 0)
    verifier.VerifyUpdate(buf, len);

  if (!verifier.VerifyFinal()) {
    ReportFailure("Signature verification failed");
    return false;
  }

  base::Base64Encode(std::string(reinterpret_cast<char*>(&key.front()),
      key.size()), &public_key_);
  return true;
}

void SandboxedExtensionUnpacker::ReportFailure(const std::string& error) {
  client_->OnUnpackFailure(error);
}

void SandboxedExtensionUnpacker::ReportSuccess() {
  // Client takes ownership of temporary directory and extension.
  client_->OnUnpackSuccess(temp_dir_.Take(), extension_root_,
                           extension_.release());
}

DictionaryValue* SandboxedExtensionUnpacker::RewriteManifestFile(
    const DictionaryValue& manifest) {
  // Add the public key extracted earlier to the parsed manifest and overwrite
  // the original manifest. We do this to ensure the manifest doesn't contain an
  // exploitable bug that could be used to compromise the browser.
  scoped_ptr<DictionaryValue> final_manifest(
      static_cast<DictionaryValue*>(manifest.DeepCopy()));
  final_manifest->SetString(extension_manifest_keys::kPublicKey, public_key_);

  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(*final_manifest)) {
    ReportFailure("Error serializing manifest.json.");
    return NULL;
  }

  FilePath manifest_path =
      extension_root_.AppendASCII(Extension::kManifestFilename);
  if (!file_util::WriteFile(manifest_path,
                            manifest_json.data(), manifest_json.size())) {
    ReportFailure("Error saving manifest.json.");
    return NULL;
  }

  return final_manifest.release();
}

bool SandboxedExtensionUnpacker::RewriteImageFiles() {
  ExtensionUnpacker::DecodedImages images;
  if (!ExtensionUnpacker::ReadImagesFromFile(temp_dir_.path(), &images)) {
    ReportFailure("Couldn't read image data from disk.");
    return false;
  }

  // Delete any images that may be used by the browser.  We're going to write
  // out our own versions of the parsed images, and we want to make sure the
  // originals are gone for good.
  std::set<FilePath> image_paths = extension_->GetBrowserImages();
  if (image_paths.size() != images.size()) {
    ReportFailure("Decoded images don't match what's in the manifest.");
    return false;
  }

  for (std::set<FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    FilePath path = *it;
    if (path.IsAbsolute() || path.ReferencesParent()) {
      ReportFailure("Invalid path for browser image.");
      return false;
    }
    if (!file_util::Delete(extension_root_.Append(path), false)) {
      ReportFailure("Error removing old image file.");
      return false;
    }
  }

  // Write our parsed images back to disk as well.
  for (size_t i = 0; i < images.size(); ++i) {
    const SkBitmap& image = images[i].a;
    FilePath path_suffix = images[i].b;
    if (path_suffix.IsAbsolute() || path_suffix.ReferencesParent()) {
      ReportFailure("Invalid path for bitmap image.");
      return false;
    }
    FilePath path = extension_root_.Append(path_suffix);

    std::vector<unsigned char> image_data;
    // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
    // though they may originally be .jpg, etc.  Figure something out.
    // http://code.google.com/p/chromium/issues/detail?id=12459
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data)) {
      ReportFailure("Error re-encoding theme image.");
      return false;
    }

    // Note: we're overwriting existing files that the utility process wrote,
    // so we can be sure the directory exists.
    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    if (!file_util::WriteFile(path, image_data_ptr, image_data.size())) {
      ReportFailure("Error saving theme image.");
      return false;
    }
  }

  return true;
}

bool SandboxedExtensionUnpacker::RewriteCatalogFiles(
    const DictionaryValue& catalogs) {
  // Write our parsed catalogs back to disk.
  for (DictionaryValue::key_iterator key_it = catalogs.begin_keys();
       key_it != catalogs.end_keys(); ++key_it) {
    DictionaryValue* catalog;
    if (!catalogs.GetDictionaryWithoutPathExpansion(*key_it, &catalog)) {
      ReportFailure("Invalid catalog data.");
      return false;
    }

    FilePath relative_path = FilePath::FromWStringHack(*key_it);
    relative_path = relative_path.AppendASCII(Extension::kMessagesFilename);
    if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
      ReportFailure("Invalid path for catalog.");
      return false;
    }
    FilePath path = extension_root_.Append(relative_path);

    std::string catalog_json;
    JSONStringValueSerializer serializer(&catalog_json);
    serializer.set_pretty_print(true);
    if (!serializer.Serialize(*catalog)) {
      ReportFailure("Error serializing catalog.");
      return false;
    }

    // Note: we're overwriting existing files that the utility process read,
    // so we can be sure the directory exists.
    if (!file_util::WriteFile(path,
                              catalog_json.c_str(),
                              catalog_json.size())) {
      ReportFailure("Error saving catalog.");
      return false;
    }
  }

  return true;
}
