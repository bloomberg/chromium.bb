// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SANDBOXED_EXTENSION_UNPACKER_H_
#define CHROME_BROWSER_EXTENSIONS_SANDBOXED_EXTENSION_UNPACKER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_temp_dir.h"
#include "chrome/browser/utility_process_host.h"

class DictionaryValue;
class Extension;
class ResourceDispatcherHost;

class SandboxedExtensionUnpackerClient
    : public base::RefCountedThreadSafe<SandboxedExtensionUnpackerClient> {
 public:
  // temp_dir - A temporary directory containing the results of the extension
  // unpacking. The client is responsible for deleting this directory.
  //
  // extension_root - The path to the extension root inside of temp_dir.
  //
  // extension - The extension that was unpacked. The client is responsible
  // for deleting this memory.
  virtual void OnUnpackSuccess(const FilePath& temp_dir,
                               const FilePath& extension_root,
                               const Extension* extension) = 0;
  virtual void OnUnpackFailure(const std::string& error) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SandboxedExtensionUnpackerClient>;

  virtual ~SandboxedExtensionUnpackerClient() {}
};

// SandboxedExtensionUnpacker unpacks extensions from the CRX format into a
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
class SandboxedExtensionUnpacker : public UtilityProcessHost::Client {
 public:
  // The size of the magic character sequence at the beginning of each crx
  // file, in bytes. This should be a multiple of 4.
  static const size_t kExtensionHeaderMagicSize = 4;

  // This header is the first data at the beginning of an extension. Its
  // contents are purposely 32-bit aligned so that it can just be slurped into
  // a struct without manual parsing.
  struct ExtensionHeader {
    char magic[kExtensionHeaderMagicSize];
    uint32 version;
    uint32 key_size;  // The size of the public key, in bytes.
    uint32 signature_size;  // The size of the signature, in bytes.
    // An ASN.1-encoded PublicKeyInfo structure follows.
    // The signature follows.
  };

  // The maximum size the crx parser will tolerate for a public key.
  static const uint32 kMaxPublicKeySize = 1 << 16;

  // The maximum size the crx parser will tolerate for a signature.
  static const uint32 kMaxSignatureSize = 1 << 16;

  // The magic character sequence at the beginning of each crx file.
  static const char kExtensionHeaderMagic[];

  // The current version of the crx format.
  static const uint32 kCurrentVersion = 2;

  // Unpacks the extension in |crx_path| into a temporary directory and calls
  // |client| with the result. If |rdh| is provided, unpacking is done in a
  // sandboxed subprocess. Otherwise, it is done in-process.
  SandboxedExtensionUnpacker(const FilePath& crx_path,
                             ResourceDispatcherHost* rdh,
                             SandboxedExtensionUnpackerClient* cilent);

  // Start unpacking the extension. The client is called with the results.
  void Start();

 private:
  class ProcessHostClient;

  // Enumerate all the ways unpacking can fail.  Calls to ReportFailure()
  // take a failure reason as an argument, and put it in histogram
  // Extensions.SandboxUnpackFailureReason.
  enum FailureReason {
    // SandboxedExtensionUnpacker::CreateTempDirectory()
    COULD_NOT_GET_TEMP_DIRECTORY,
    COULD_NOT_CREATE_TEMP_DIRECTORY,

    // SandboxedExtensionUnpacker::Start()
    FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
    COULD_NOT_GET_SANDBOX_FRIENDLY_PATH,

    // SandboxedExtensionUnpacker::OnUnpackExtensionSucceeded()
    COULD_NOT_LOCALIZE_EXTENSION,
    INVALID_MANIFEST,

    //SandboxedExtensionUnpacker::OnUnpackExtensionFailed()
    UNPACKER_CLIENT_FAILED,

    // SandboxedExtensionUnpacker::OnProcessCrashed()
    UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,

    // SandboxedExtensionUnpacker::ValidateSignature()
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

    // SandboxedExtensionUnpacker::RewriteManifestFile()
    ERROR_SERIALIZING_MANIFEST_JSON,
    ERROR_SAVING_MANIFEST_JSON,

    // SandboxedExtensionUnpacker::RewriteImageFiles()
    COULD_NOT_READ_IMAGE_DATA_FROM_DISK,
    DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST,
    INVALID_PATH_FOR_BROWSER_IMAGE,
    ERROR_REMOVING_OLD_IMAGE_FILE,
    INVALID_PATH_FOR_BITMAP_IMAGE,
    ERROR_RE_ENCODING_THEME_IMAGE,
    ERROR_SAVING_THEME_IMAGE,

    // SandboxedExtensionUnpacker::RewriteCatalogFiles()
    COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
    INVALID_CATALOG_DATA,
    INVALID_PATH_FOR_CATALOG,
    ERROR_SERIALIZING_CATALOG,
    ERROR_SAVING_CATALOG,

    NUM_FAILURE_REASONS
  };

  friend class ProcessHostClient;
  friend class SandboxedExtensionUnpackerTest;

  virtual ~SandboxedExtensionUnpacker();

  // Set |temp_dir_| as a temporary directory to unpack the extension in.
  // Return true on success.
  virtual bool CreateTempDirectory();

  // Validates the signature of the extension and extract the key to
  // |public_key_|. Returns true if the signature validates, false otherwise.
  //
  // NOTE: Having this method here is a bit ugly. This code should really live
  // in ExtensionUnpacker as it is not specific to sandboxed unpacking. It was
  // put here because we cannot run windows crypto code in the sandbox. But we
  // could still have this method statically on ExtensionUnpacker so that code
  // just for unpacking is there and code just for sandboxing of unpacking is
  // here.
  bool ValidateSignature();

  // Starts the utility process that unpacks our extension.
  void StartProcessOnIOThread(const FilePath& temp_crx_path);

  // SandboxedExtensionUnpacker
  virtual void OnUnpackExtensionSucceeded(const DictionaryValue& manifest);
  virtual void OnUnpackExtensionFailed(const std::string& error_message);
  virtual void OnProcessCrashed(int exit_code);

  void ReportFailure(FailureReason reason, const std::string& message);
  void ReportSuccess();

  // Overwrites original manifest with safe result from utility process.
  // Returns NULL on error. Caller owns the returned object.
  DictionaryValue* RewriteManifestFile(const DictionaryValue& manifest);

  // Overwrites original files with safe results from utility process.
  // Reports error and returns false if it fails.
  bool RewriteImageFiles();
  bool RewriteCatalogFiles();

  // The path to the CRX to unpack.
  FilePath crx_path_;

  // Our client's thread. This is the thread we respond on.
  BrowserThread::ID thread_identifier_;

  // ResourceDispatcherHost to pass to the utility process.
  ResourceDispatcherHost* rdh_;

  // Our client.
  scoped_refptr<SandboxedExtensionUnpackerClient> client_;

  // A temporary directory to use for unpacking.
  ScopedTempDir temp_dir_;

  // The root directory of the unpacked extension. This is a child of temp_dir_.
  FilePath extension_root_;

  // Represents the extension we're unpacking.
  scoped_refptr<Extension> extension_;

  // Whether we've received a response from the utility process yet.
  bool got_response_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_SANDBOXED_EXTENSION_UNPACKER_H_
