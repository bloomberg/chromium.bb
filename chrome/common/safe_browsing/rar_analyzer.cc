// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/rar_analyzer.h"

#include <memory>
#include <string>
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "third_party/unrar/src/unrar_wrapper.h"

namespace safe_browsing {
namespace rar_analyzer {

void AnalyzeRarFile(base::File rar_file,
                    const base::FilePath& rar_file_path,
                    ArchiveAnalyzerResults* results) {
  auto archive = std::make_unique<third_party_unrar::Archive>();
  archive->SetFileHandle(rar_file.GetPlatformFile());
  std::wstring wide_rar_file_path(rar_file_path.value().begin(),
                                  rar_file_path.value().end());
  if (!archive->Open(wide_rar_file_path.c_str())) {
    results->success = false;
    // TODO(vakh): Log UMA here.
    VLOG(1) << __FUNCTION__
            << ": Unable to open rar_file: " << rar_file.GetPlatformFile();
    return;
  }
  if (!archive->IsArchive(/*EnableBroken=*/true)) {
    // TODO(vakh): Log UMA here.
    results->success = false;
    VLOG(1) << __FUNCTION__
            << ": !IsArchive: rar_file: " << rar_file.GetPlatformFile();
    return;
  }
  // TODO(vakh): Log UMA here.

  results->success = true;
  std::set<base::FilePath> archived_archive_filenames;
  for (archive->ViewComment();
       archive->ReadHeader() > 0 &&
       archive->GetHeaderType() != third_party_unrar::kUnrarEndarcHead;
       archive->SeekToNext()) {
    VLOG(2) << __FUNCTION__ << ": FileName: " << archive->FileHead.FileName;
    std::wstring wide_filename(archive->FileHead.FileName);
#if defined(OS_WIN)
    base::FilePath file_path(wide_filename);
#else
    std::string filename(wide_filename.begin(), wide_filename.end());
    base::FilePath file_path(filename);
#endif  // OS_WIN
    VLOG(2) << __FUNCTION__ << ": file_path: " << file_path.value().c_str();

    bool is_executable =
        FileTypePolicies::GetInstance()->IsCheckedBinaryFile(file_path);
    VLOG(2) << __FUNCTION__ << ": is_executable: " << is_executable;
    results->has_executable |= is_executable;

    bool is_archive = FileTypePolicies::GetInstance()->IsArchiveFile(file_path);
    VLOG(2) << __FUNCTION__ << ": is_archive: " << is_archive;
    results->has_archive |= is_archive;
    if (is_archive) {
      archived_archive_filenames.insert(file_path.BaseName());
    }
    results->archived_archive_filenames.assign(
        archived_archive_filenames.begin(), archived_archive_filenames.end());
  }
}

}  // namespace rar_analyzer
}  // namespace safe_browsing
