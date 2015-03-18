// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/drive_metrics_provider.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

DriveMetricsProvider::DriveMetricsProvider() : weak_ptr_factory_(this) {}

DriveMetricsProvider::~DriveMetricsProvider() {}

void DriveMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  auto* hardware = system_profile_proto->mutable_hardware();
  FillDriveMetrics(metrics_.app_drive, hardware->mutable_app_drive());
  FillDriveMetrics(metrics_.user_data_drive,
                   hardware->mutable_user_data_drive());
}

void DriveMetricsProvider::GetDriveMetrics(const base::Closure& done) {
  got_metrics_callback_ = done;

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DriveMetricsProvider::GetDriveMetricsOnFileThread),
      base::Bind(&DriveMetricsProvider::GotDriveMetrics,
                 weak_ptr_factory_.GetWeakPtr()));
}

DriveMetricsProvider::SeekPenaltyResponse::SeekPenaltyResponse()
    : success(false) {}

// static
DriveMetricsProvider::DriveMetrics
DriveMetricsProvider::GetDriveMetricsOnFileThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  DriveMetricsProvider::DriveMetrics metrics;
  QuerySeekPenalty(base::FILE_EXE, &metrics.app_drive);
  QuerySeekPenalty(chrome::FILE_LOCAL_STATE, &metrics.user_data_drive);
  return metrics;
}

// static
void DriveMetricsProvider::QuerySeekPenalty(
    int path_service_key,
    DriveMetricsProvider::SeekPenaltyResponse* response) {
  DCHECK(response);

  base::FilePath path;
  if (!PathService::Get(path_service_key, &path))
    return;

  response->success =
      base::SysInfo::HasSeekPenalty(path, &response->has_seek_penalty);
}

void DriveMetricsProvider::GotDriveMetrics(
    const DriveMetricsProvider::DriveMetrics& metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics_ = metrics;
  got_metrics_callback_.Run();
}

void DriveMetricsProvider::FillDriveMetrics(
    const DriveMetricsProvider::SeekPenaltyResponse& response,
    metrics::SystemProfileProto::Hardware::Drive* drive) {
  if (response.success)
    drive->set_has_seek_penalty(response.has_seek_penalty);
}
