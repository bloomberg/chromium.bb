// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_importer_impl.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/common/chrome_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

const base::FilePath::CharType kMHTMLExtension[] = FILE_PATH_LITERAL("mhtml");

// Mirror of the OfflinePrefetchPageImportResult histogram enum so existing
// entries can never be removed and new ones must be appended with new values.
enum class PageImportResult {
  SUCCESS = 0,
  UNKNOWN = 1,
  FILE_MOVE_ERROR = 2,
  OFFLINE_STORE_FAILURE = 3,
  OFFLINE_ITEM_ALREADY_EXISTS = 4,
  // Always leave this item last. Update if the actual last item changes.
  MAX = OFFLINE_ITEM_ALREADY_EXISTS
};

PageImportResult FromAddPageResult(AddPageResult result) {
  switch (result) {
    case AddPageResult::SUCCESS:
      return PageImportResult::SUCCESS;
    case AddPageResult::STORE_FAILURE:
      return PageImportResult::OFFLINE_STORE_FAILURE;
    case AddPageResult::ALREADY_EXISTS:
      return PageImportResult::OFFLINE_ITEM_ALREADY_EXISTS;
    case AddPageResult::RESULT_COUNT:
      NOTREACHED();
  }
  NOTREACHED();
  return PageImportResult::UNKNOWN;
}

void ReportPageImportResult(PageImportResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.Prefetching.OfflinePageImportResult",
                            result, PageImportResult::MAX);
}

void MoveFile(const base::FilePath& src_path,
              const base::FilePath& dest_path,
              base::TaskRunner* task_runner,
              const base::Callback<void(bool)>& callback) {
  bool success = base::Move(src_path, dest_path);
  task_runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

}  // namespace

PrefetchImporterImpl::PrefetchImporterImpl(
    PrefetchDispatcher* dispatcher,
    content::BrowserContext* context,
    scoped_refptr<base::TaskRunner> background_task_runner)
    : PrefetchImporter(dispatcher),
      context_(context),
      background_task_runner_(background_task_runner),
      weak_ptr_factory_(this) {}

PrefetchImporterImpl::~PrefetchImporterImpl() = default;

void PrefetchImporterImpl::ImportArchive(const PrefetchArchiveInfo& archive) {
  // The target file name will be auto generated based on GUID to prevent any
  // name collision.
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(context_);
  DCHECK(offline_page_model);

  base::FilePath archives_dir =
      offline_page_model->GetArchiveDirectory(archive.client_id.name_space);
  base::FilePath dest_path =
      archives_dir.Append(base::GenerateGUID()).AddExtension(kMHTMLExtension);

  // For PrefetchArchiveInfo, |url| is the original URL while
  // |final_archived_url| is the last committed URL which is set to empty when
  // there is no redirect.
  //
  // For OfflinePageItem, |url| is the last committed URL while |original_url|
  // is the original URL which is set to empty when there is no redirect.
  //
  // So when |PrefetchArchiveInfo.final_archived_url| is empty,
  // |PrefetchArchiveInfo.url| denotes the sole URL which should be passed to
  // |OfflinePageItem.url|. Otherwise, we should switch the urls for
  // OfflinePageItem.
  GURL url, original_url;
  if (archive.final_archived_url.is_empty()) {
    url = archive.url;
  } else {
    url = archive.final_archived_url;
    original_url = archive.url;
  }
  OfflinePageItem offline_page(url, archive.offline_id, archive.client_id,
                               dest_path, archive.file_size, base::Time::Now());
  offline_page.original_url = original_url;
  offline_page.title = archive.title;

  // Moves the file from download directory to offline archive directory. The
  // file move operation should be done on background thread.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MoveFile, archive.file_path, dest_path,
                 base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                 base::Bind(&PrefetchImporterImpl::OnMoveFileDone,
                            weak_ptr_factory_.GetWeakPtr(), offline_page)));
}

void PrefetchImporterImpl::OnMoveFileDone(const OfflinePageItem& offline_page,
                                          bool success) {
  if (!success) {
    ReportPageImportResult(PageImportResult::FILE_MOVE_ERROR);
    NotifyImportCompleted(OfflinePageModel::kInvalidOfflineId, false);
    return;
  }

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(context_);
  DCHECK(offline_page_model);

  offline_page_model->AddPage(offline_page,
                              base::Bind(&PrefetchImporterImpl::OnPageAdded,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PrefetchImporterImpl::OnPageAdded(AddPageResult result,
                                       int64_t offline_id) {
  ReportPageImportResult(FromAddPageResult(result));
  NotifyImportCompleted(offline_id, result == AddPageResult::SUCCESS);
}

}  // namespace offline_pages
