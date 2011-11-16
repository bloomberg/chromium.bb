// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"

using content::BrowserThread;

SaveFile::SaveFile(const SaveFileCreateInfo* info)
    : file_(FilePath(), info->url, GURL(), 0, linked_ptr<net::FileStream>()),
      info_(info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DCHECK(info);
  DCHECK(info->path.empty());
}

SaveFile::~SaveFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

net::Error SaveFile::Initialize(bool calculate_hash) {
  return file_.Initialize(calculate_hash);
}

net::Error SaveFile::AppendDataToFile(const char* data, size_t data_len) {
  return file_.AppendDataToFile(data, data_len);
}

net::Error SaveFile::Rename(const FilePath& full_path) {
  return file_.Rename(full_path);
}

void SaveFile::Detach() {
  file_.Detach();
}

void SaveFile::Cancel() {
  file_.Cancel();
}

void SaveFile::Finish() {
  file_.Finish();
}

void SaveFile::AnnotateWithSourceInformation() {
  file_.AnnotateWithSourceInformation();
}

FilePath SaveFile::FullPath() const {
  return file_.full_path();
}

bool SaveFile::InProgress() const {
  return file_.in_progress();
}

int64 SaveFile::BytesSoFar() const {
  return file_.bytes_so_far();
}

bool SaveFile::GetSha256Hash(std::string* hash) {
  return file_.GetSha256Hash(hash);
}

std::string SaveFile::DebugString() const {
  return file_.DebugString();
}
