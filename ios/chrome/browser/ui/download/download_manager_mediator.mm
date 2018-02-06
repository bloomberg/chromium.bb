// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_mediator.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/web/public/download/download_task.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DownloadManagerMediator::DownloadManagerMediator() : weak_ptr_factory_(this) {}
DownloadManagerMediator::~DownloadManagerMediator() {}

void DownloadManagerMediator::SetConsumer(
    id<DownloadManagerConsumer> consumer) {
  consumer_ = consumer;
  UpdateConsumer();
}

void DownloadManagerMediator::SetDownloadTask(web::DownloadTask* task) {
  if (task_) {
    task_->RemoveObserver(this);
  }
  task_ = task;
  if (task_) {
    UpdateConsumer();
    task_->AddObserver(this);
  }
}

void DownloadManagerMediator::StartDowloading() {
  base::FilePath download_dir;
  if (!GetDownloadsDirectory(&download_dir)) {
    [consumer_ setState:kDownloadManagerStateFailed];
    return;
  }

  // Download will start once writer is created by background task, however it
  // OK to change consumer state now to preven further user interactions with
  // "Start Download" button.
  [consumer_ setState:kDownloadManagerStateInProgress];

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::CreateDirectory, download_dir),
      base::BindOnce(&DownloadManagerMediator::DownloadWithDestinationDir,
                     weak_ptr_factory_.GetWeakPtr(), download_dir));
}

void DownloadManagerMediator::DownloadWithDestinationDir(
    const base::FilePath& destination_dir,
    bool directory_created) {
  if (!directory_created) {
    [consumer_ setState:kDownloadManagerStateFailed];
    return;
  }

  auto task_runner = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BACKGROUND});
  base::string16 file_name = task_->GetSuggestedFilename();
  base::FilePath path = destination_dir.Append(base::UTF16ToUTF8(file_name));
  auto writer = std::make_unique<net::URLFetcherFileWriter>(task_runner, path);
  writer->Initialize(base::BindRepeating(
      &DownloadManagerMediator::DownloadWithWriter,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(std::move(writer))));
}

void DownloadManagerMediator::DownloadWithWriter(
    std::unique_ptr<net::URLFetcherFileWriter> writer,
    int writer_initialization_status) {
  if (writer_initialization_status == net::OK) {
    task_->Start(std::move(writer));
  } else {
    [consumer_ setState:kDownloadManagerStateFailed];
  }
}

void DownloadManagerMediator::OnDownloadUpdated(web::DownloadTask* task) {
  UpdateConsumer();
}

void DownloadManagerMediator::UpdateConsumer() {
  [consumer_ setState:GetDownloadManagerState()];
  [consumer_ setCountOfBytesReceived:task_->GetReceivedBytes()];
  [consumer_ setCountOfBytesExpectedToReceive:task_->GetTotalBytes()];
  [consumer_
      setFileName:base::SysUTF16ToNSString(task_->GetSuggestedFilename())];
}

DownloadManagerState DownloadManagerMediator::GetDownloadManagerState() {
  switch (task_->GetState()) {
    case web::DownloadTask::State::kNotStarted:
      return kDownloadManagerStateNotStarted;
    case web::DownloadTask::State::kInProgress:
      return kDownloadManagerStateInProgress;
    case web::DownloadTask::State::kComplete:
      return task_->GetErrorCode() ? kDownloadManagerStateFailed
                                   : kDownloadManagerStateSuceeded;
    case web::DownloadTask::State::kCancelled:
      // Download Manager should dismiss the UI after download cancellation.
      return kDownloadManagerStateNotStarted;
  }
}
