// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/upload_list.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"

UploadList::UploadInfo::UploadInfo(const std::string& id,
                                   const base::Time& t,
                                   const std::string& local_id)
    : id(id), time(t), local_id(local_id) {}

UploadList::UploadInfo::UploadInfo(const std::string& id, const base::Time& t)
    : id(id), time(t) {}

UploadList::UploadInfo::~UploadInfo() {}

UploadList::UploadList(
    Delegate* delegate,
    const base::FilePath& upload_log_path,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool)
    : delegate_(delegate),
      upload_log_path_(upload_log_path),
      worker_pool_(worker_pool) {}

UploadList::~UploadList() {}

void UploadList::LoadUploadListAsynchronously() {
  DCHECK(thread_checker_.CalledOnValidThread());
  worker_pool_->PostTask(
      FROM_HERE,
      base::Bind(&UploadList::LoadUploadListAndInformDelegateOfCompletion,
                 this, base::ThreadTaskRunnerHandle::Get()));
}

void UploadList::ClearDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_ = NULL;
}

void UploadList::LoadUploadListAndInformDelegateOfCompletion(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  LoadUploadList();
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&UploadList::InformDelegateOfCompletion, this));
}

void UploadList::LoadUploadList() {
  if (base::PathExists(upload_log_path_)) {
    std::string contents;
    base::ReadFileToString(upload_log_path_, &contents);
    std::vector<std::string> log_entries;
    base::SplitStringAlongWhitespace(contents, &log_entries);
    ClearUploads();
    ParseLogEntries(log_entries);
  }
}

void UploadList::AppendUploadInfo(const UploadInfo& info) {
  uploads_.push_back(info);
}

void UploadList::ClearUploads() {
  uploads_.clear();
}

void UploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries) {
  std::vector<std::string>::const_reverse_iterator i;
  for (i = log_entries.rbegin(); i != log_entries.rend(); ++i) {
    std::vector<std::string> components = base::SplitString(
        *i, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    // Skip any blank (or corrupted) lines.
    if (components.size() != 2 && components.size() != 3)
      continue;
    base::Time upload_time;
    double seconds_since_epoch;
    if (!components[0].empty()) {
      if (!base::StringToDouble(components[0], &seconds_since_epoch))
        continue;
      upload_time = base::Time::FromDoubleT(seconds_since_epoch);
    }
    UploadInfo info(components[1], upload_time);
    if (components.size() == 3)
      info.local_id = components[2];
    uploads_.push_back(info);
  }
}

void UploadList::InformDelegateOfCompletion() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_)
    delegate_->OnUploadListAvailable();
}

void UploadList::GetUploads(unsigned int max_count,
                            std::vector<UploadInfo>* uploads) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::copy(uploads_.begin(),
            uploads_.begin() + std::min<size_t>(uploads_.size(), max_count),
            std::back_inserter(*uploads));
}
