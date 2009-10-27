// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_UI_H_
#define CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_UI_H_

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"

class MessageLoop;

// Manages packing an extension on the file thread and reporting the result
// back to the UI.
class PackExtensionJob : public base::RefCountedThreadSafe<PackExtensionJob> {
 public:

  // Interface for people who want to use PackExtensionJob to implement.
  class Client {
   public:
    virtual void OnPackSuccess(const FilePath& crx_file,
                               const FilePath& key_file) = 0;
    virtual void OnPackFailure(const std::wstring& message) = 0;
  };

  PackExtensionJob(Client* client,
                   const FilePath& root_directory,
                   const FilePath& key_file);

  // The client should call this when it is destroyed to prevent
  // PackExtensionJob from attempting to access it.
  void ClearClient();

 private:
  void RunOnFileThread();
  void ReportSuccessOnUIThread();
  void ReportFailureOnUIThread(const std::string& error);

  MessageLoop* ui_loop_;
  Client* client_;
  FilePath root_directory_;
  FilePath key_file_;
  FilePath crx_file_out_;
  FilePath key_file_out_;

  DISALLOW_COPY_AND_ASSIGN(PackExtensionJob);
};

#endif  // CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_UI_H_

