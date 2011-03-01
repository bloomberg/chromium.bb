// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/file_reader.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread.h"
#include "chrome/common/extensions/extension_resource.h"

FileReader::FileReader(const ExtensionResource& resource, Callback* callback)
    : resource_(resource),
      callback_(callback),
      origin_loop_(MessageLoop::current()) {
  DCHECK(callback_);
}

void FileReader::Start() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &FileReader::ReadFileOnBackgroundThread));
}

FileReader::~FileReader() {}

void FileReader::ReadFileOnBackgroundThread() {
  std::string data;
  bool success = file_util::ReadFileToString(resource_.GetFilePath(), &data);
  origin_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &FileReader::RunCallback, success, data));
}

void FileReader::RunCallback(bool success, const std::string& data) {
  callback_->Run(success, data);
  delete callback_;
}
