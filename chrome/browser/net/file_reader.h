// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines an interface for reading a file asynchronously on a
// background thread.

#ifndef CHROME_BROWSER_NET_FILE_READER_H_
#define CHROME_BROWSER_NET_FILE_READER_H_

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/task.h"

class MessageLoop;

class FileReader : public base::RefCountedThreadSafe<FileReader> {
 public:
  // Reports success or failure and the data of the file upon success.
  typedef Callback2<bool, const std::string&>::Type Callback;

  FileReader(const FilePath& path, Callback* callback);

  // Called to start reading the file on a background thread.  Upon completion,
  // the callback will be notified of the results.
  void Start();

 private:
  void ReadFileOnBackgroundThread();
  void RunCallback(bool success, const std::string& data);

  FilePath path_;
  Callback* callback_;
  MessageLoop* origin_loop_;
};

#endif  // CHROME_BROWSER_NET_FILE_READER_H_
