// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UTILITY_PROCESS_HOST_H_
#define CHROME_BROWSER_UTILITY_PROCESS_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/extensions/update_manifest.h"
#include "ipc/ipc_channel.h"

class CommandLine;
class DictionaryValue;
class ListValue;

// This class acts as the browser-side host to a utility child process.  A
// utility process is a short-lived sandboxed process that is created to run
// a specific task.  This class lives solely on the IO thread.
class UtilityProcessHost : public ChildProcessHost {
 public:
  // An interface to be implemented by consumers of the utility process to
  // get results back.  All functions are called on the thread passed along
  // to UtilityProcessHost.
  class Client : public base::RefCountedThreadSafe<Client> {
   public:
    Client() {}

    // Called when the process has crashed.
    virtual void OnProcessCrashed() {}

    // Called when the extension has unpacked successfully.  |manifest| is the
    // parsed manifest.json file.  |catalogs| contains list of all parsed
    // message catalogs.  |images| contains a list of decoded images and the
    // associated paths where those images live on disk.
    virtual void OnUnpackExtensionSucceeded(const DictionaryValue& manifest,
                                            const DictionaryValue& catalogs) {}

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
        const UpdateManifest::ResultList& list) {}

    // Called when an update manifest xml file failed parsing. |error_message|
    // contains details suitable for logging.
    virtual void OnParseUpdateManifestFailed(
        const std::string& error_message) {}

   protected:
    friend class base::RefCountedThreadSafe<Client>;

    virtual ~Client() {}

   private:
    friend class UtilityProcessHost;

    void OnMessageReceived(const IPC::Message& message);

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  UtilityProcessHost(ResourceDispatcherHost* rdh, Client* client,
                     ChromeThread::ID client_thread_id);
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

 protected:
  // Allow these methods to be overridden for tests.
  virtual FilePath GetUtilityProcessCmd();

 private:
  // Starts a process.  Returns true iff it succeeded.
  bool StartProcess(const FilePath& exposed_dir);

  // IPC messages:
  void OnMessageReceived(const IPC::Message& message);

  // ChildProcessHost:
  virtual void OnChannelError();
  virtual bool CanShutdown() { return true; }
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data) {
    return NULL;
  }

  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<Client> client_;
  ChromeThread::ID client_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHost);
};

#endif  // CHROME_BROWSER_UTILITY_PROCESS_HOST_H_
