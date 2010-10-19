// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SANDBOXED_EXTENSION_UNPACKER_H_
#define CHROME_BROWSER_EXTENSIONS_SANDBOXED_EXTENSION_UNPACKER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
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
                               Extension* extension) = 0;
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
                             const FilePath& temp_path,
                             ResourceDispatcherHost* rdh,
                             SandboxedExtensionUnpackerClient* cilent);

  // Start unpacking the extension. The client is called with the results.
  void Start();

 private:
  class ProcessHostClient;
  friend class ProcessHostClient;
  friend class SandboxedExtensionUnpackerTest;

  virtual ~SandboxedExtensionUnpacker();

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
  void OnUnpackExtensionSucceeded(const DictionaryValue& manifest);
  void OnUnpackExtensionFailed(const std::string& error_message);
  void OnProcessCrashed();

  void ReportFailure(const std::string& message);
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

  // A path to a temp dir to unpack in.
  FilePath temp_path_;

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
  scoped_ptr<Extension> extension_;

  // Whether we've received a response from the utility process yet.
  bool got_response_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_SANDBOXED_EXTENSION_UNPACKER_H_
