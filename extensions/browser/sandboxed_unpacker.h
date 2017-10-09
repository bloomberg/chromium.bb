// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
#define EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/manifest.h"

class SkBitmap;

namespace base {
class DictionaryValue;
class ListValue;
class SequencedTaskRunner;
}

namespace extensions {
class Extension;

namespace mojom {
class ExtensionUnpacker;
}

class SandboxedUnpackerClient
    : public base::RefCountedDeleteOnSequence<SandboxedUnpackerClient> {
 public:
  // Initialize the ref-counted base to always delete on the UI thread. Note
  // the constructor call must also happen on the UI thread.
  SandboxedUnpackerClient();

  // temp_dir - A temporary directory containing the results of the extension
  // unpacking. The client is responsible for deleting this directory.
  //
  // extension_root - The path to the extension root inside of temp_dir.
  //
  // original_manifest - The parsed but unmodified version of the manifest,
  // with no modifications such as localization, etc.
  //
  // extension - The extension that was unpacked. The client is responsible
  // for deleting this memory.
  //
  // install_icon - The icon we will display in the installation UI, if any.
  //
  // dnr_ruleset_checksum - Checksum for the indexed ruleset corresponding to
  // the Declarative Net Request API. Optional since it's only valid for
  // extensions which provide a declarative ruleset.
  virtual void OnUnpackSuccess(
      const base::FilePath& temp_dir,
      const base::FilePath& extension_root,
      std::unique_ptr<base::DictionaryValue> original_manifest,
      const Extension* extension,
      const SkBitmap& install_icon,
      const base::Optional<int>& dnr_ruleset_checksum) = 0;
  virtual void OnUnpackFailure(const CrxInstallError& error) = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<SandboxedUnpackerClient>;
  friend class base::DeleteHelper<SandboxedUnpackerClient>;

  virtual ~SandboxedUnpackerClient() = default;
};

// SandboxedUnpacker does work to optionally unpack and then validate/sanitize
// an extension, either starting from a crx file, or else an already unzipped
// directory (eg., from a differential update). This is done in a sandboxed
// subprocess to protect the browser process from parsing complex data formats
// like JPEG or JSON from untrusted sources.
//
// Unpacking an extension using this class makes changes to its source, such as
// transcoding all images to PNG, parsing all message catalogs, and rewriting
// the manifest JSON. As such, it should not be used when the output is not
// intended to be given back to the author.
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessMojoClient.
//
// Additionally, we hold a reference to our own client so that the client lives
// long enough to receive the result of unpacking.
//
// NOTE: This class should only be used on the FILE thread.
//
class SandboxedUnpacker : public base::RefCountedThreadSafe<SandboxedUnpacker> {
 public:
  // Creates a SanboxedUnpacker that will do work to unpack an extension,
  // passing the |location| and |creation_flags| to Extension::Create. The
  // |extensions_dir| parameter should specify the directory under which we'll
  // create a subdirectory to write the unpacked extension contents.
  // Note: Because this requires disk I/O, the task runner passed should use
  // TaskShutdownBehavior::SKIP_ON_SHUTDOWN to ensure that either the task is
  // fully run (if initiated before shutdown) or not run at all (if shutdown is
  // initiated first). See crbug.com/235525.
  // TODO(devlin): We should probably just have SandboxedUnpacker use the common
  // ExtensionFileTaskRunner, and not pass in a separate one.
  // TODO(devlin): SKIP_ON_SHUTDOWN is also not quite sufficient for this. We
  // should probably instead be using base::ImportantFileWriter or similar.
  SandboxedUnpacker(
      Manifest::Location location,
      int creation_flags,
      const base::FilePath& extensions_dir,
      const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
      SandboxedUnpackerClient* client);

  // Start processing the extension, either from a CRX file or already unzipped
  // in a directory. The client is called with the results. The directory form
  // requires the id and base64-encoded public key (for insertion into the
  // 'key' field of the manifest.json file).
  void StartWithCrx(const CRXFileInfo& crx_info);
  void StartWithDirectory(const std::string& extension_id,
                          const std::string& public_key_base64,
                          const base::FilePath& directory);

 private:
  friend class base::RefCountedThreadSafe<SandboxedUnpacker>;

  // Enumerate all the ways unpacking can fail.  Calls to ReportFailure()
  // take a failure reason as an argument, and put it in histogram
  // Extensions.SandboxUnpackFailureReason.
  enum FailureReason {
    // SandboxedUnpacker::CreateTempDirectory()
    COULD_NOT_GET_TEMP_DIRECTORY,
    COULD_NOT_CREATE_TEMP_DIRECTORY,

    // SandboxedUnpacker::Start()
    FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
    COULD_NOT_GET_SANDBOX_FRIENDLY_PATH,

    // SandboxedUnpacker::UnpackExtensionSucceeded()
    COULD_NOT_LOCALIZE_EXTENSION,
    INVALID_MANIFEST,

    // SandboxedUnpacker::UnpackExtensionFailed()
    UNPACKER_CLIENT_FAILED,

    // SandboxedUnpacker::UtilityProcessCrashed()
    UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,

    // SandboxedUnpacker::ValidateSignature()
    CRX_FILE_NOT_READABLE,
    CRX_HEADER_INVALID,
    CRX_MAGIC_NUMBER_INVALID,
    CRX_VERSION_NUMBER_INVALID,
    CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE,
    CRX_ZERO_KEY_LENGTH,
    CRX_ZERO_SIGNATURE_LENGTH,
    CRX_PUBLIC_KEY_INVALID,
    CRX_SIGNATURE_INVALID,
    CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED,
    CRX_SIGNATURE_VERIFICATION_FAILED,

    // SandboxedUnpacker::RewriteManifestFile()
    ERROR_SERIALIZING_MANIFEST_JSON,
    ERROR_SAVING_MANIFEST_JSON,

    // SandboxedUnpacker::RewriteImageFiles()
    COULD_NOT_READ_IMAGE_DATA_FROM_DISK,
    DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST,
    INVALID_PATH_FOR_BROWSER_IMAGE,
    ERROR_REMOVING_OLD_IMAGE_FILE,
    INVALID_PATH_FOR_BITMAP_IMAGE,
    ERROR_RE_ENCODING_THEME_IMAGE,
    ERROR_SAVING_THEME_IMAGE,
    DEPRECATED_ABORTED_DUE_TO_SHUTDOWN,  // No longer used; kept for UMA.

    // SandboxedUnpacker::RewriteCatalogFiles()
    COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
    INVALID_CATALOG_DATA,
    INVALID_PATH_FOR_CATALOG,
    ERROR_SERIALIZING_CATALOG,
    ERROR_SAVING_CATALOG,

    // SandboxedUnpacker::ValidateSignature()
    CRX_HASH_VERIFICATION_FAILED,

    UNZIP_FAILED,
    DIRECTORY_MOVE_FAILED,

    // SandboxedUnpacker::ValidateSignature()
    CRX_FILE_IS_DELTA_UPDATE,
    CRX_EXPECTED_HASH_INVALID,

    // SandboxedUnpacker::IndexAndPersistRulesIfNeeded()
    ERROR_PARSING_DNR_RULESET,
    ERROR_INDEXING_DNR_RULESET,

    NUM_FAILURE_REASONS
  };

  friend class SandboxedUnpackerTest;

  ~SandboxedUnpacker();

  // Create |temp_dir_| used to unzip or unpack the extension in.
  bool CreateTempDirectory();

  // Helper functions to simplify calling ReportFailure.
  base::string16 FailureReasonToString16(FailureReason reason);
  void FailWithPackageError(FailureReason reason);

  // Validates the signature of the extension and extract the key to
  // |public_key_|. True if the signature validates, false otherwise.
  bool ValidateSignature(const base::FilePath& crx_path,
                         const std::string& expected_hash);

  // Ensures the utility process is created.
  void StartUtilityProcessIfNeeded();

  // Utility process crashed or failed while trying to install.
  void UtilityProcessCrashed();

  // Unzips the extension into directory.
  void Unzip(const base::FilePath& crx_path);
  void UnzipDone(const base::FilePath& directory, bool success);

  // Unpacks the extension in directory and returns the manifest.
  void Unpack(const base::FilePath& directory);
  void UnpackDone(const base::string16& error,
                  std::unique_ptr<base::DictionaryValue> manifest,
                  std::unique_ptr<base::ListValue> json_ruleset);
  void UnpackExtensionSucceeded(std::unique_ptr<base::DictionaryValue> manifest,
                                std::unique_ptr<base::ListValue> json_ruleset);
  void UnpackExtensionFailed(const base::string16& error);

  // Reports unpack success or failure, or unzip failure.
  void ReportSuccess(std::unique_ptr<base::DictionaryValue> original_manifest,
                     const SkBitmap& install_icon,
                     const base::Optional<int>& dnr_ruleset_checksum);
  void ReportFailure(FailureReason reason, const base::string16& error);

  // Overwrites original manifest with safe result from utility process.
  // Returns NULL on error. Caller owns the returned object.
  base::DictionaryValue* RewriteManifestFile(
      const base::DictionaryValue& manifest);

  // Overwrites original files with safe results from utility process.
  // Reports error and returns false if it fails.
  bool RewriteImageFiles(SkBitmap* install_icon);
  bool RewriteCatalogFiles();

  // Cleans up temp directory artifacts.
  void Cleanup();

  // Indexes |json_ruleset| if it is non-null and persists the corresponding
  // indexed file for the Declarative Net Request API. Also, returns the
  // checksum of the indexed ruleset file on success. Returns false and reports
  // failure in case of an error.
  bool IndexAndPersistRulesIfNeeded(
      std::unique_ptr<base::ListValue> json_ruleset,
      int* dnr_ruleset_checksum);

  // If we unpacked a CRX file, we hold on to the path name for use
  // in various histograms.
  base::FilePath crx_path_for_histograms_;

  // Our unpacker client.
  scoped_refptr<SandboxedUnpackerClient> client_;

  // The Extensions directory inside the profile.
  base::FilePath extensions_dir_;

  // Temporary directory to use for unpacking.
  base::ScopedTempDir temp_dir_;

  // Root directory of the unpacked extension (a child of temp_dir_).
  base::FilePath extension_root_;

  // Represents the extension we're unpacking.
  scoped_refptr<Extension> extension_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;

  // The extension's ID. This will be calculated from the public key
  // in the CRX header.
  std::string extension_id_;

  // If we unpacked a CRX file, the time at which unpacking started.
  // Used to compute the time unpacking takes.
  base::TimeTicks crx_unpack_start_time_;

  // Location to use for the unpacked extension.
  Manifest::Location location_;

  // Creation flags to use for the extension. These flags will be used
  // when calling Extenion::Create() by the CRX installer.
  int creation_flags_;

  // Sequenced task runner where file I/O operations will be performed.
  scoped_refptr<base::SequencedTaskRunner> unpacker_io_task_runner_;

  // Utility client used for sending tasks to the utility process.
  std::unique_ptr<content::UtilityProcessMojoClient<mojom::ExtensionUnpacker>>
      utility_process_mojo_client_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedUnpacker);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
