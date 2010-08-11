// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
#define CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"


// Manages packing an extension on the file thread and reporting the result
// back to the UI.
class PackExtensionJob : public base::RefCountedThreadSafe<PackExtensionJob> {
 public:

  // Interface for people who want to use PackExtensionJob to implement.
  class Client {
   public:
    virtual void OnPackSuccess(const FilePath& crx_file,
                               const FilePath& key_file) = 0;
    virtual void OnPackFailure(const std::string& message) = 0;

   protected:
    virtual ~Client() {}
  };

  PackExtensionJob(Client* client,
                   const FilePath& root_directory,
                   const FilePath& key_file);

  // Starts the packing thread job. See http://crbug.com/27944 for more details
  // on why this function is needed.
  void Start();

  // The client should call this when it is destroyed to prevent
  // PackExtensionJob from attempting to access it.
  void ClearClient();

  // The standard packing success message.
  static std::wstring StandardSuccessMessage(const FilePath& crx_file,
                                             const FilePath& key_file);

 private:
  friend class base::RefCountedThreadSafe<PackExtensionJob>;

  ~PackExtensionJob() {}

  void RunOnFileThread();
  void ReportSuccessOnClientThread();
  void ReportFailureOnClientThread(const std::string& error);

  ChromeThread::ID client_thread_id_;
  Client* client_;
  FilePath root_directory_;
  FilePath key_file_;
  FilePath crx_file_out_;
  FilePath key_file_out_;

  DISALLOW_COPY_AND_ASSIGN(PackExtensionJob);
};

#endif  // CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
