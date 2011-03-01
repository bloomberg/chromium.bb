// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UTILITY_PROCESS_HOST_H_
#define CHROME_BROWSER_UTILITY_PROCESS_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/common/extensions/update_manifest.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_thread.h"

class DictionaryValue;
class IndexedDBKey;
class SerializedScriptValue;
class SkBitmap;

// This class acts as the browser-side host to a utility child process.  A
// utility process is a short-lived sandboxed process that is created to run
// a specific task.  This class lives solely on the IO thread.
// If you need a single method call in the sandbox, use StartFooBar(p).
// If you need multiple batches of work to be done in the sandboxed process,
// use StartBatchMode(), then multiple calls to StartFooBar(p),
// then finish with EndBatchMode().
class UtilityProcessHost : public BrowserChildProcessHost {
 public:
  // An interface to be implemented by consumers of the utility process to
  // get results back.  All functions are called on the thread passed along
  // to UtilityProcessHost.
  class Client : public base::RefCountedThreadSafe<Client> {
   public:
    Client() {}

    // Called when the process has crashed.
    virtual void OnProcessCrashed(int exit_code) {}

    // Called when the extension has unpacked successfully.  |manifest| is the
    // parsed manifest.json file.  |catalogs| contains list of all parsed
    // message catalogs.  |images| contains a list of decoded images and the
    // associated paths where those images live on disk.
    virtual void OnUnpackExtensionSucceeded(const DictionaryValue& manifest) {}

    // Called when an error occurred while unpacking the extension.
    // |error_message| contains a description of the problem.
    virtual void OnUnpackExtensionFailed(const std::string& error_message) {}

    // Called when the web resource has been successfully parsed.  |json_data|
    // contains the parsed list of web resource items downloaded from the
    // web resource server.
    virtual void OnUnpackWebResourceSucceeded(
        const DictionaryValue& json_data) {}

    // Called when an error occurred while parsing the resource data.
    // |error_message| contains a description of the problem.
    virtual void OnUnpackWebResourceFailed(const std::string& error_message) {}

    // Called when an update manifest xml file was successfully parsed.
    virtual void OnParseUpdateManifestSucceeded(
        const UpdateManifest::Results& results) {}

    // Called when an update manifest xml file failed parsing. |error_message|
    // contains details suitable for logging.
    virtual void OnParseUpdateManifestFailed(
        const std::string& error_message) {}

    // Called when image data was successfully decoded. |decoded_image|
    // stores the result.
    virtual void OnDecodeImageSucceeded(
        const SkBitmap& decoded_image) {}

    // Called when image data decoding failed.
    virtual void OnDecodeImageFailed() {}

    // Called when we have successfully obtained the IndexedDBKey after
    // a call to StartIDBKeysFromValuesAndKeyPath.
    // |id| is the corresponding identifier.
    // |keys| the corresponding IndexedDBKey.
    virtual void OnIDBKeysFromValuesAndKeyPathSucceeded(
        int id, const std::vector<IndexedDBKey>& keys) {}

    // Called when IDBKeyPath has failed.
    // |id| is the corresponding identifier passed on
    // StartIDBKeysFromValuesAndKeyPath.
    virtual void OnIDBKeysFromValuesAndKeyPathFailed(int id) {}

    // Called when an IDBKey was injected into a
    // SerializedScriptValue. If injection failed, SerializedScriptValue is
    // empty.
    virtual void OnInjectIDBKeyFinished(
        const SerializedScriptValue& new_value) {}

   protected:
    friend class base::RefCountedThreadSafe<Client>;

    virtual ~Client() {}

   private:
    friend class UtilityProcessHost;

    bool OnMessageReceived(const IPC::Message& message);

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  UtilityProcessHost(ResourceDispatcherHost* rdh, Client* client,
                     BrowserThread::ID client_thread_id);
  virtual ~UtilityProcessHost();

  // Start a process to unpack the extension at the given path.  The process
  // will be given access to the directory subtree that the extension file is
  // in, so the caller is expected to have moved that file into a quarantined
  // location first.
  bool StartExtensionUnpacker(const FilePath& extension);

  // Start a process to unpack and parse a web resource from the given JSON
  // data.  Any links that need to be downloaded from the parsed data
  // (thumbnails, etc.) will be unpacked in resource_dir.
  // TODO(mrc): Right now, the unpacker just parses the JSON data, and
  // doesn't do any unpacking.  This should change once we finalize the
  // web resource server format(s).
  bool StartWebResourceUnpacker(const std::string& data);

  // Start parsing an extensions auto-update manifest xml file.
  bool StartUpdateManifestParse(const std::string& xml);

  // Start image decoding.
  bool StartImageDecoding(const std::vector<unsigned char>& encoded_data);

  // Starts extracting |key_path| from |serialized_values|, and replies with the
  // corresponding IndexedDBKeys via OnIDBKeysFromValuesAndKeyPathSucceeded.
  bool StartIDBKeysFromValuesAndKeyPath(
      int id, const std::vector<SerializedScriptValue>& serialized_values,
      const string16& key_path);

  // Starts injecting |key| into |value| via |key_path|, and replies with the
  // updated value via OnInjectIDBKeyFinished.
  bool StartInjectIDBKey(const IndexedDBKey& key,
                         const SerializedScriptValue& value,
                         const string16& key_path);

  // Starts utility process in batch mode. Caller must call EndBatchMode()
  // to finish the utility process.
  bool StartBatchMode();

  // Ends the utility process. Must be called after StartBatchMode().
  void EndBatchMode();

 protected:
  // Allow these methods to be overridden for tests.
  virtual FilePath GetUtilityProcessCmd();

 private:
  // Starts a process if necessary.  Returns true if it succeeded or a process
  // has already been started via StartBatchMode().
  bool StartProcess(const FilePath& exposed_dir);

  // IPC messages:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // BrowserChildProcessHost:
  virtual void OnProcessCrashed(int exit_code);
  virtual bool CanShutdown();

  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<Client> client_;
  BrowserThread::ID client_thread_id_;
  // True when running in batch mode, i.e., StartBatchMode() has been called
  // and the utility process will run until EndBatchMode().
  bool is_batch_mode_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHost);
};

#endif  // CHROME_BROWSER_UTILITY_PROCESS_HOST_H_
