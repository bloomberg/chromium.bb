// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SANDBOXED_UNPACKER_H_
#define CHROME_BROWSER_EXTENSIONS_SANDBOXED_UNPACKER_H_

#include <string>

#include "base/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}

namespace extensions {

class SandboxedUnpackerClient
    : public base::RefCountedThreadSafe<SandboxedUnpackerClient> {
 public:
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
  virtual void OnUnpackSuccess(const FilePath& temp_dir,
                               const FilePath& extension_root,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension) = 0;
  virtual void OnUnpackFailure(const string16& error) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SandboxedUnpackerClient>;

  virtual ~SandboxedUnpackerClient() {}
};

// SandboxedUnpacker unpacks extensions from the CRX format into a
// directory. This is done in a sandboxed subprocess to protect the browser
// process from parsing complex formats like JPEG or JSON from untrusted
// sources.
//
// Unpacking an extension using this class makes minor changes to its source,
// such as transcoding all images to PNG, parsing all message catalogs
// and rewriting the manifest JSON. As such, it should not be used when the
// output is not intended to be given back to the author.
//
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of unpacking.
//
//
// NOTE: This class should only be used on the file thread.
class SandboxedUnpacker : public content::UtilityProcessHostClient {
 public:
  // Unpacks the extension in |crx_path| into a temporary directory and calls
  // |client| with the result. If |run_out_of_process| is provided, unpacking
  // is done in a sandboxed subprocess. Otherwise, it is done in-process.
  SandboxedUnpacker(const FilePath& crx_path,
                    bool run_out_of_process,
                    Extension::Location location,
                    int creation_flags,
                    const FilePath& extensions_dir,
                    base::SequencedTaskRunner* unpacker_io_task_runner,
                    SandboxedUnpackerClient* client);

  // Start unpacking the extension. The client is called with the results.
  void Start();

 private:
  class ProcessHostClient;

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

    // SandboxedUnpacker::OnUnpackExtensionSucceeded()
    COULD_NOT_LOCALIZE_EXTENSION,
    INVALID_MANIFEST,

    // SandboxedUnpacker::OnUnpackExtensionFailed()
    UNPACKER_CLIENT_FAILED,

    // SandboxedUnpacker::OnProcessCrashed()
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

    // SandboxedUnpacker::RewriteCatalogFiles()
    COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
    INVALID_CATALOG_DATA,
    INVALID_PATH_FOR_CATALOG,
    ERROR_SERIALIZING_CATALOG,
    ERROR_SAVING_CATALOG,

    NUM_FAILURE_REASONS
  };

  friend class ProcessHostClient;
  friend class SandboxedUnpackerTest;

  virtual ~SandboxedUnpacker();

  // Set |temp_dir_| as a temporary directory to unpack the extension in.
  // Return true on success.
  virtual bool CreateTempDirectory();

  // Validates the signature of the extension and extract the key to
  // |public_key_|. Returns true if the signature validates, false otherwise.
  //
  // NOTE: Having this method here is a bit ugly. This code should really live
  // in extensions::Unpacker as it is not specific to sandboxed unpacking. It
  // was put here because we cannot run windows crypto code in the sandbox. But
  // we could still have this method statically on extensions::Unpacker so that
  // code just for unpacking is there and code just for sandboxing of unpacking
  // is here.
  bool ValidateSignature();

  // Starts the utility process that unpacks our extension.
  void StartProcessOnIOThread(const FilePath& temp_crx_path);

  // UtilityProcessHostClient
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // IPC message handlers.
  void OnUnpackExtensionSucceeded(const base::DictionaryValue& manifest);
  void OnUnpackExtensionFailed(const string16& error_message);

  void ReportFailure(FailureReason reason, const string16& message);
  void ReportSuccess(const base::DictionaryValue& original_manifest);

  // Overwrites original manifest with safe result from utility process.
  // Returns NULL on error. Caller owns the returned object.
  base::DictionaryValue* RewriteManifestFile(
      const base::DictionaryValue& manifest);

  // Overwrites original files with safe results from utility process.
  // Reports error and returns false if it fails.
  bool RewriteImageFiles();
  bool RewriteCatalogFiles();

  // Cleans up temp directory artifacts.
  void Cleanup();

  // The path to the CRX to unpack.
  FilePath crx_path_;

  // True if unpacking should be done by the utility process.
  bool run_out_of_process_;

  // Our client.
  scoped_refptr<SandboxedUnpackerClient> client_;

  // The Extensions directory inside the profile.
  FilePath extensions_dir_;

  // A temporary directory to use for unpacking.
  base::ScopedTempDir temp_dir_;

  // The root directory of the unpacked extension. This is a child of temp_dir_.
  FilePath extension_root_;

  // Represents the extension we're unpacking.
  scoped_refptr<Extension> extension_;

  // Whether we've received a response from the utility process yet.
  bool got_response_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;

  // The extension's ID. This will be calculated from the public key in the crx
  // header.
  std::string extension_id_;

  // Time at which unpacking started. Used to compute the time unpacking takes.
  base::TimeTicks unpack_start_time_;

  // Location to use for the unpacked extension.
  Extension::Location location_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extenion::Create() by the crx installer.
  int creation_flags_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> unpacker_io_task_runner_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SANDBOXED_UNPACKER_H_
