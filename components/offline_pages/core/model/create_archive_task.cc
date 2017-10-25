// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/create_archive_task.h"

#include <memory>

#include "base/bind.h"
#include "base/time/default_clock.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;

CreateArchiveTask::CreateArchiveTask(
    const base::FilePath& archives_dir,
    const OfflinePageModel::SavePageParams& save_page_params,
    OfflinePageArchiver* archiver,
    const CreateArchiveTaskCallback& callback)
    : archives_dir_(archives_dir),
      save_page_params_(save_page_params),
      archiver_(archiver),
      callback_(callback),
      clock_(new base::DefaultClock()),
      skip_clearing_original_url_for_testing_(false) {}

CreateArchiveTask::~CreateArchiveTask() {}

void CreateArchiveTask::Run() {
  CreateArchive();
}

void CreateArchiveTask::CreateArchive() {
  OfflinePageItem proposed_page;
  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!OfflinePageModel::CanSaveURL(save_page_params_.url)) {
    InformCreateArchiveFailed(ArchiverResult::ERROR_SKIPPED);
    return;
  }

  // The web contents is not available if archiver is not created and passed.
  if (!archiver_) {
    InformCreateArchiveFailed(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  // If we already have an offline id, use it. If not, generate one.
  int64_t offline_id = save_page_params_.proposed_offline_id;
  if (offline_id == OfflinePageModel::kInvalidOfflineId)
    offline_id = store_utils::GenerateOfflineId();

  // Create a proposed OfflinePageItem to pass in callback, the page will be
  // missing fields, which are going to be filled when the archive creation
  // finishes.
  proposed_page.url = save_page_params_.url;
  proposed_page.offline_id = offline_id;
  proposed_page.client_id = save_page_params_.client_id;
  proposed_page.creation_time = clock_->Now();
  proposed_page.request_origin = save_page_params_.request_origin;

  // Don't record the original URL if it is identical to the final URL. This is
  // because some websites might route the redirect finally back to itself upon
  // the completion of certain action, i.e., authentication, in the middle.
  if (skip_clearing_original_url_for_testing_ ||
      save_page_params_.original_url != proposed_page.url) {
    proposed_page.original_url = save_page_params_.original_url;
  }

  OfflinePageArchiver::CreateArchiveParams create_archive_params;
  // If the page is being saved in the background, we should try to remove the
  // popup overlay that obstructs viewing the normal content.
  create_archive_params.remove_popup_overlay = save_page_params_.is_background;
  create_archive_params.use_page_problem_detectors =
      save_page_params_.use_page_problem_detectors;
  archiver_->CreateArchive(archives_dir_, create_archive_params,
                           base::Bind(callback_, proposed_page));

  // The task will complete here, and the callback will be called once the
  // |archiver_| is done with the archive creation. This enables multiple
  // archive creation going on by not blocking the TaskQueue waiting for the
  // completion.
  TaskComplete();
}

void CreateArchiveTask::InformCreateArchiveFailed(ArchiverResult result) {
  callback_.Run(OfflinePageItem(), archiver_, result, GURL(), base::FilePath(),
                base::string16(), 0);
  TaskComplete();
}

}  // namespace offline_pages
