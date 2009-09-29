// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/file_reader.h"

#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"

FileReader::FileReader(const FilePath& path, Callback* callback)
    : path_(path),
      callback_(callback),
      origin_loop_(MessageLoop::current()) {
  DCHECK(callback_);
}

void FileReader::Start() {
  ChromeThread::GetMessageLoop(ChromeThread::FILE)->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FileReader::ReadFileOnBackgroundThread));
}

void FileReader::ReadFileOnBackgroundThread() {
  std::string data;
  bool success = file_util::ReadFileToString(path_, &data);
  origin_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &FileReader::RunCallback, success, data));
}

void FileReader::RunCallback(bool success, const std::string& data) {
  callback_->Run(success, data);
  delete callback_;
}
