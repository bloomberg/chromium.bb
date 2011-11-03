// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/file_reader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

FileReader::FileReader(const ExtensionResource& resource,
                       const Callback& callback)
    : resource_(resource),
      callback_(callback),
      origin_loop_(MessageLoop::current()) {
}

void FileReader::Start() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileReader::ReadFileOnBackgroundThread, this));
}

FileReader::~FileReader() {}

void FileReader::ReadFileOnBackgroundThread() {
  std::string data;
  bool success = file_util::ReadFileToString(resource_.GetFilePath(), &data);
  origin_loop_->PostTask(FROM_HERE, base::Bind(callback_, success, data));
}
