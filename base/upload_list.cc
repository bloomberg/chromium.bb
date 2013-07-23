// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/upload_list.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/sequenced_worker_pool.h"

namespace base {

UploadList::UploadInfo::UploadInfo(const std::string& c, const Time& t)
    : id(c), time(t) {}

UploadList::UploadInfo::~UploadInfo() {}

UploadList::UploadList(Delegate* delegate,
                       const FilePath& upload_log_path,
                       SingleThreadTaskRunner* task_runner)
    : delegate_(delegate),
      upload_log_path_(upload_log_path),
      task_runner_(task_runner) {}

UploadList::~UploadList() {}

void UploadList::LoadUploadListAsynchronously(
    base::SequencedWorkerPool* worker_pool) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  worker_pool->PostWorkerTask(
      FROM_HERE,
      Bind(&UploadList::LoadUploadListAndInformDelegateOfCompletion, this));
}

void UploadList::ClearDelegate() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  delegate_ = NULL;
}

void UploadList::LoadUploadListAndInformDelegateOfCompletion() {
  LoadUploadList();
  task_runner_->PostTask(
      FROM_HERE, Bind(&UploadList::InformDelegateOfCompletion, this));
}

void UploadList::LoadUploadList() {
  if (PathExists(upload_log_path_)) {
    std::string contents;
    file_util::ReadFileToString(upload_log_path_, &contents);
    std::vector<std::string> log_entries;
    SplitStringAlongWhitespace(contents, &log_entries);
    ParseLogEntries(log_entries);
  }
}

void UploadList::AppendUploadInfo(const UploadInfo& info) {
  uploads_.push_back(info);
}

void UploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries) {
  std::vector<std::string>::const_reverse_iterator i;
  for (i = log_entries.rbegin(); i != log_entries.rend(); ++i) {
    std::vector<std::string> components;
    SplitString(*i, ',', &components);
    // Skip any blank (or corrupted) lines.
    if (components.size() != 2)
      continue;
    double seconds_since_epoch;
    if (!StringToDouble(components[0], &seconds_since_epoch))
      continue;
    UploadInfo info(components[1], Time::FromDoubleT(seconds_since_epoch));
    uploads_.push_back(info);
  }
}

void UploadList::InformDelegateOfCompletion() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (delegate_)
    delegate_->OnUploadListAvailable();
}

void UploadList::GetUploads(unsigned int max_count,
                            std::vector<UploadInfo>* uploads) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  std::copy(uploads_.begin(),
            uploads_.begin() + std::min<size_t>(uploads_.size(), max_count),
            std::back_inserter(*uploads));
}

}  // namespace base
