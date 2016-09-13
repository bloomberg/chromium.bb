// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/media_licenses_counter.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace {

// Determining the origins must be run on the file task thread.
std::set<GURL> CountOriginsOnFileTaskRunner(
    storage::FileSystemContext* filesystem_context) {
  DCHECK(filesystem_context->default_file_task_runner()
             ->RunsTasksOnCurrentThread());

  storage::FileSystemBackend* backend =
      filesystem_context->GetFileSystemBackend(
          storage::kFileSystemTypePluginPrivate);
  storage::FileSystemQuotaUtil* quota_util = backend->GetQuotaUtil();

  std::set<GURL> origins;
  quota_util->GetOriginsForTypeOnFileTaskRunner(
      storage::kFileSystemTypePluginPrivate, &origins);
  return origins;
}

}  // namespace

MediaLicensesCounter::MediaLicenseResult::MediaLicenseResult(
    const MediaLicensesCounter* source,
    const std::set<GURL>& origins)
    : FinishedResult(source, origins.size()) {
  if (!origins.empty())
    one_origin_ = origins.begin()->GetOrigin().host();
}

MediaLicensesCounter::MediaLicenseResult::~MediaLicenseResult() {}

const std::string& MediaLicensesCounter::MediaLicenseResult::GetOneOrigin()
    const {
  return one_origin_;
}

MediaLicensesCounter::MediaLicensesCounter(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {}

MediaLicensesCounter::~MediaLicensesCounter() {}

const char* MediaLicensesCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteMediaLicenses;
}

void MediaLicensesCounter::Count() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<storage::FileSystemContext> filesystem_context =
      make_scoped_refptr(
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetFileSystemContext());
  base::PostTaskAndReplyWithResult(
      filesystem_context->default_file_task_runner(), FROM_HERE,
      base::Bind(&CountOriginsOnFileTaskRunner,
                 base::RetainedRef(filesystem_context)),
      base::Bind(&MediaLicensesCounter::OnContentLicensesObtained,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MediaLicensesCounter::OnContentLicensesObtained(
    const std::set<GURL>& origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportResult(base::MakeUnique<MediaLicenseResult>(this, origins));
}
