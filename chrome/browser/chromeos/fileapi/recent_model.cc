// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_model.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/fileapi/recent_arc_media_source.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/browser/chromeos/fileapi/recent_download_source.h"
#include "chrome/browser/chromeos/fileapi/recent_drive_source.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model_factory.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_context.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Recent file cache will be cleared this duration after it is built.
constexpr base::TimeDelta kCacheExpiration = base::TimeDelta::FromSeconds(10);

std::vector<std::unique_ptr<RecentSource>> CreateDefaultSources(
    Profile* profile) {
  std::vector<std::unique_ptr<RecentSource>> sources;
  sources.emplace_back(base::MakeUnique<RecentArcMediaSource>(profile));
  sources.emplace_back(base::MakeUnique<RecentDownloadSource>(profile));
  sources.emplace_back(base::MakeUnique<RecentDriveSource>(profile));
  return sources;
}

}  // namespace

const size_t kMaxFilesFromSingleSource = 1000;

const char RecentModel::kLoadHistogramName[] = "FileBrowser.Recent.LoadTotal";

// static
RecentModel* RecentModel::GetForProfile(Profile* profile) {
  return RecentModelFactory::GetForProfile(profile);
}

// static
std::unique_ptr<RecentModel> RecentModel::CreateForTest(
    std::vector<std::unique_ptr<RecentSource>> sources) {
  return base::WrapUnique(new RecentModel(std::move(sources)));
}

RecentModel::RecentModel(Profile* profile)
    : RecentModel(CreateDefaultSources(profile)) {}

RecentModel::RecentModel(std::vector<std::unique_ptr<RecentSource>> sources)
    : sources_(std::move(sources)), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentModel::~RecentModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sources_.empty());
}

void RecentModel::GetRecentFiles(const RecentContext& context,
                                 GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Use cache if available.
  if (cached_urls_.has_value()) {
    std::move(callback).Run(cached_urls_.value());
    return;
  }

  bool builder_already_running = !pending_callbacks_.empty();
  pending_callbacks_.emplace_back(std::move(callback));

  // If a builder is already running, just enqueue the callback and return.
  if (builder_already_running)
    return;

  // Start building a recent file list.
  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(intermediate_files_.empty());
  DCHECK(build_start_time_.is_null());

  build_start_time_ = base::TimeTicks::Now();

  num_inflight_sources_ = sources_.size();
  if (sources_.empty()) {
    OnGetRecentFilesCompleted();
    return;
  }

  for (const auto& source : sources_) {
    source->GetRecentFiles(
        context, base::BindOnce(&RecentModel::OnGetRecentFiles,
                                weak_ptr_factory_.GetWeakPtr(), context));
  }
}

void RecentModel::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some RecentSource implementations have references to other KeyedServices,
  // so we destruct them here.
  sources_.clear();
}

void RecentModel::OnGetRecentFiles(const RecentContext& context,
                                   std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_LT(0, num_inflight_sources_);

  for (const auto& file : files)
    intermediate_files_.emplace(file);

  while (intermediate_files_.size() > kMaxFilesFromSingleSource)
    intermediate_files_.pop();

  --num_inflight_sources_;
  if (num_inflight_sources_ == 0)
    OnGetRecentFilesCompleted();
}

void RecentModel::OnGetRecentFilesCompleted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(!cached_urls_.has_value());
  DCHECK(!build_start_time_.is_null());

  std::vector<storage::FileSystemURL> urls;
  while (!intermediate_files_.empty()) {
    urls.emplace_back(intermediate_files_.top().url());
    intermediate_files_.pop();
  }
  std::reverse(urls.begin(), urls.end());
  cached_urls_ = std::move(urls);

  DCHECK(cached_urls_.has_value());
  DCHECK(intermediate_files_.empty());

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  // Starts a timer to clear cache.
  cache_clear_timer_.Start(
      FROM_HERE, kCacheExpiration,
      base::Bind(&RecentModel::ClearCache, weak_ptr_factory_.GetWeakPtr()));

  // Invoke all pending callbacks.
  std::vector<GetRecentFilesCallback> callbacks_to_call;
  callbacks_to_call.swap(pending_callbacks_);
  DCHECK(pending_callbacks_.empty());
  DCHECK(!callbacks_to_call.empty());
  for (auto& callback : callbacks_to_call)
    std::move(callback).Run(cached_urls_.value());
}

void RecentModel::ClearCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  cached_urls_.reset();
}

}  // namespace chromeos
