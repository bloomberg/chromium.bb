// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_creator.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/extension_creator_filter.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/crx2_file.h"
#include "components/crx_file/id_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
  const int kRSAKeySize = 2048;
};

namespace extensions {

ExtensionCreator::ExtensionCreator() : error_type_(kOtherError) {
}

bool ExtensionCreator::InitializeInput(
    const base::FilePath& extension_dir,
    const base::FilePath& crx_path,
    const base::FilePath& private_key_path,
    const base::FilePath& private_key_output_path,
    int run_flags) {
  // Validate input |extension_dir|.
  if (extension_dir.value().empty() ||
      !base::DirectoryExists(extension_dir)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_DIRECTORY_NO_EXISTS);
    return false;
  }

  base::FilePath absolute_extension_dir =
      base::MakeAbsoluteFilePath(extension_dir);
  if (absolute_extension_dir.empty()) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_CANT_GET_ABSOLUTE_PATH);
    return false;
  }

  // Validate input |private_key| (if provided).
  if (!private_key_path.value().empty() &&
      !base::PathExists(private_key_path)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID_PATH);
    return false;
  }

  // If an |output_private_key| path is given, make sure it doesn't over-write
  // an existing private key.
  if (private_key_path.value().empty() &&
      !private_key_output_path.value().empty() &&
      base::PathExists(private_key_output_path)) {
      error_message_ =
          l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_EXISTS);
      return false;
  }

  // Check whether crx file already exists. Should be last check, as this is
  // a warning only.
  if (!(run_flags & kOverwriteCRX) && base::PathExists(crx_path)) {
    error_message_ = l10n_util::GetStringUTF8(IDS_EXTENSION_CRX_EXISTS);
    error_type_ = kCRXExists;

    return false;
  }

  return true;
}

bool ExtensionCreator::ValidateManifest(const base::FilePath& extension_dir,
                                        crypto::RSAPrivateKey* key_pair,
                                        int run_flags) {
  std::vector<uint8_t> public_key_bytes;
  if (!key_pair->ExportPublicKey(&public_key_bytes)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PUBLIC_KEY_FAILED_TO_EXPORT);
    return false;
  }

  std::string public_key;
  public_key.insert(public_key.begin(),
                    public_key_bytes.begin(), public_key_bytes.end());

  std::string extension_id = crx_file::id_util::GenerateId(public_key);

  // Load the extension once. We don't really need it, but this does a lot of
  // useful validation of the structure.
  int create_flags =
      Extension::FOLLOW_SYMLINKS_ANYWHERE | Extension::ERROR_ON_PRIVATE_KEY;
  if (run_flags & kRequireModernManifestVersion)
    create_flags |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;

  scoped_refptr<Extension> extension(
      file_util::LoadExtension(extension_dir,
                               extension_id,
                               Manifest::INTERNAL,
                               create_flags,
                               &error_message_));
  return !!extension.get();
}

std::unique_ptr<crypto::RSAPrivateKey> ExtensionCreator::ReadInputKey(
    const base::FilePath& private_key_path) {
  if (!base::PathExists(private_key_path)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_NO_EXISTS);
    return NULL;
  }

  std::string private_key_contents;
  if (!base::ReadFileToString(private_key_path, &private_key_contents)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_READ);
    return NULL;
  }

  std::string private_key_bytes;
  if (!Extension::ParsePEMKeyBytes(private_key_contents,
       &private_key_bytes)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID);
    return NULL;
  }

  return crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
      std::vector<uint8_t>(private_key_bytes.begin(), private_key_bytes.end()));
}

std::unique_ptr<crypto::RSAPrivateKey> ExtensionCreator::GenerateKey(
    const base::FilePath& output_private_key_path) {
  std::unique_ptr<crypto::RSAPrivateKey> key_pair(
      crypto::RSAPrivateKey::Create(kRSAKeySize));
  if (!key_pair) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_GENERATE);
    return NULL;
  }

  std::vector<uint8_t> private_key_vector;
  if (!key_pair->ExportPrivateKey(&private_key_vector)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_EXPORT);
    return NULL;
  }
  std::string private_key_bytes(
      reinterpret_cast<char*>(&private_key_vector.front()),
      private_key_vector.size());

  std::string private_key;
  if (!Extension::ProducePEM(private_key_bytes, &private_key)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_OUTPUT);
    return NULL;
  }
  std::string pem_output;
  if (!Extension::FormatPEMForFileOutput(private_key, &pem_output,
       false)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_OUTPUT);
    return NULL;
  }

  if (!output_private_key_path.empty()) {
    if (-1 == base::WriteFile(output_private_key_path,
        pem_output.c_str(), pem_output.size())) {
      error_message_ =
          l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_OUTPUT);
      return NULL;
    }
  }

  return key_pair;
}

bool ExtensionCreator::CreateZip(const base::FilePath& extension_dir,
                                 const base::FilePath& temp_path,
                                 base::FilePath* zip_path) {
  *zip_path = temp_path.Append(FILE_PATH_LITERAL("extension.zip"));

  scoped_refptr<ExtensionCreatorFilter> filter = new ExtensionCreatorFilter();
  const base::Callback<bool(const base::FilePath&)>& filter_cb =
    base::Bind(&ExtensionCreatorFilter::ShouldPackageFile, filter);
  if (!zip::ZipWithFilterCallback(extension_dir, *zip_path, filter_cb)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_FAILED_DURING_PACKAGING);
    return false;
  }

  return true;
}

bool ExtensionCreator::SignZip(const base::FilePath& zip_path,
                               crypto::RSAPrivateKey* private_key,
                               std::vector<uint8_t>* signature) {
  std::unique_ptr<crypto::SignatureCreator> signature_creator(
      crypto::SignatureCreator::Create(private_key,
                                       crypto::SignatureCreator::SHA1));
  base::ScopedFILE zip_handle(base::OpenFile(zip_path, "rb"));
  size_t buffer_size = 1 << 16;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  int bytes_read = -1;
  while ((bytes_read = fread(buffer.get(), 1, buffer_size,
       zip_handle.get())) > 0) {
    if (!signature_creator->Update(buffer.get(), bytes_read)) {
      error_message_ =
          l10n_util::GetStringUTF8(IDS_EXTENSION_ERROR_WHILE_SIGNING);
      return false;
    }
  }
  zip_handle.reset();

  if (!signature_creator->Final(signature)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_ERROR_WHILE_SIGNING);
    return false;
  }
  return true;
}

bool ExtensionCreator::WriteCRX(const base::FilePath& zip_path,
                                crypto::RSAPrivateKey* private_key,
                                const std::vector<uint8_t>& signature,
                                const base::FilePath& crx_path) {
  if (base::PathExists(crx_path))
    base::DeleteFile(crx_path, false);
  base::ScopedFILE crx_handle(base::OpenFile(crx_path, "wb"));
  if (!crx_handle.get()) {
    error_message_ = l10n_util::GetStringUTF8(IDS_EXTENSION_SHARING_VIOLATION);
    return false;
  }

  std::vector<uint8_t> public_key;
  CHECK(private_key->ExportPublicKey(&public_key));

  crx_file::Crx2File::Error error = crx_file::Crx2File::kMaxValue;
  std::unique_ptr<crx_file::Crx2File> crx(
      crx_file::Crx2File::Create(public_key.size(), signature.size(), &error));
  if (!crx) {
    LOG(ERROR) << "cannot create Crx2FileHeader: " << error;
    return false;
  }
  const crx_file::Crx2File::Header header = crx->header();

  if (fwrite(&header, sizeof(header), 1, crx_handle.get()) != 1) {
    PLOG(ERROR) << "fwrite failed to write header";
    return false;
  }
  if (fwrite(&public_key.front(), sizeof(uint8_t), public_key.size(),
             crx_handle.get()) != public_key.size()) {
    PLOG(ERROR) << "fwrite failed to write public_key.front";
    return false;
  }
  if (fwrite(&signature.front(), sizeof(uint8_t), signature.size(),
             crx_handle.get()) != signature.size()) {
    PLOG(ERROR) << "fwrite failed to write signature.front";
    return false;
  }

  size_t buffer_size = 1 << 16;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  size_t bytes_read = 0;
  base::ScopedFILE zip_handle(base::OpenFile(zip_path, "rb"));
  while ((bytes_read = fread(buffer.get(), 1, buffer_size,
                             zip_handle.get())) > 0) {
    if (fwrite(buffer.get(), sizeof(char), bytes_read, crx_handle.get()) !=
        bytes_read) {
      PLOG(ERROR) << "fwrite failed to write buffer";
      return false;
    }
  }

  return true;
}

bool ExtensionCreator::Run(const base::FilePath& extension_dir,
                           const base::FilePath& crx_path,
                           const base::FilePath& private_key_path,
                           const base::FilePath& output_private_key_path,
                           int run_flags) {
  // Check input diretory and read manifest.
  if (!InitializeInput(extension_dir, crx_path, private_key_path,
                       output_private_key_path, run_flags)) {
    return false;
  }

  // Initialize Key Pair
  std::unique_ptr<crypto::RSAPrivateKey> key_pair;
  if (!private_key_path.value().empty())
    key_pair = ReadInputKey(private_key_path);
  else
    key_pair = GenerateKey(output_private_key_path);
  if (!key_pair)
    return false;

  // Perform some extra validation by loading the extension.
  // TODO(aa): Can this go before creating the key pair? This would mean not
  // passing ID into LoadExtension which seems OK.
  if (!ValidateManifest(extension_dir, key_pair.get(), run_flags))
    return false;

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  // Zip up the extension.
  base::FilePath zip_path;
  std::vector<uint8_t> signature;
  bool result = false;
  if (CreateZip(extension_dir, temp_dir.GetPath(), &zip_path) &&
      SignZip(zip_path, key_pair.get(), &signature) &&
      WriteCRX(zip_path, key_pair.get(), signature, crx_path)) {
    result = true;
  }

  base::DeleteFile(zip_path, false);
  return result;
}

}  // namespace extensions
