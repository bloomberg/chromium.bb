// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
#define CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// Manages packing an extension on the file thread and reporting the result
// back to the UI.
class PackExtensionJob : public base::RefCountedThreadSafe<PackExtensionJob> {
 public:
  // Interface for people who want to use PackExtensionJob to implement.
  class Client {
   public:
    virtual void OnPackSuccess(const base::FilePath& crx_file,
                               const base::FilePath& key_file) = 0;
    virtual void OnPackFailure(const std::string& message,
                               ExtensionCreator::ErrorType error_type) = 0;

   protected:
    virtual ~Client() {}
  };

  PackExtensionJob(Client* client,
                   const base::FilePath& root_directory,
                   const base::FilePath& key_file,
                   int run_flags);

  // Starts the packing job.
  void Start();

  // The client should call this when it is destroyed to prevent
  // PackExtensionJob from attempting to access it.
  void ClearClient();

  // The standard packing success message.
  static string16 StandardSuccessMessage(const base::FilePath& crx_file,
                                         const base::FilePath& key_file);

  void set_asynchronous(bool async) { asynchronous_ = async; }

 private:
  friend class base::RefCountedThreadSafe<PackExtensionJob>;

  virtual ~PackExtensionJob();

  // If |asynchronous_| is false, this is run on whichever thread calls it.
  void Run();
  void ReportSuccessOnClientThread();
  void ReportFailureOnClientThread(const std::string& error,
                                   ExtensionCreator::ErrorType error_type);

  content::BrowserThread::ID client_thread_id_;
  Client* client_;
  base::FilePath root_directory_;
  base::FilePath key_file_;
  base::FilePath crx_file_out_;
  base::FilePath key_file_out_;
  bool asynchronous_;
  int run_flags_;  // Bitset of ExtensionCreator::RunFlags values - we always
                   // assume kRequireModernManifestVersion, though.

  DISALLOW_COPY_AND_ASSIGN(PackExtensionJob);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
