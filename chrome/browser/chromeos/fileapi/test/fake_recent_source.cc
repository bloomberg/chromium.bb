// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/test/fake_recent_source.h"

#include <utility>

#include "chrome/browser/chromeos/fileapi/recent_context.h"

namespace chromeos {

FakeRecentSource::FakeRecentSource() = default;

FakeRecentSource::~FakeRecentSource() = default;

void FakeRecentSource::AddFile(const storage::FileSystemURL& file) {
  canned_files_.emplace_back(file);
}

void FakeRecentSource::GetRecentFiles(RecentContext context,
                                      GetRecentFilesCallback callback) {
  std::move(callback).Run(canned_files_);
}

}  // namespace chromeos
