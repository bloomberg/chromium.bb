// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FILE_READER_H_
#define CHROME_BROWSER_EXTENSIONS_FILE_READER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension_resource.h"

class MessageLoop;

// This file defines an interface for reading a file asynchronously on a
// background thread.
// Consider abstracting out a FilePathProvider (ExtensionResource) and moving
// back to chrome/browser/net if other subsystems want to use it.
class FileReader : public base::RefCountedThreadSafe<FileReader> {
 public:
  // Reports success or failure and the data of the file upon success.
  typedef Callback2<bool, const std::string&>::Type Callback;

  FileReader(const ExtensionResource& resource, Callback* callback);

  // Called to start reading the file on a background thread.  Upon completion,
  // the callback will be notified of the results.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<FileReader>;

  virtual ~FileReader();

  void ReadFileOnBackgroundThread();
  void RunCallback(bool success, const std::string& data);

  ExtensionResource resource_;
  Callback* callback_;
  MessageLoop* origin_loop_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_FILE_READER_H_
