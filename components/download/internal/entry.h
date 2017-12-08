// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_H_

#include "base/time/time.h"
#include "components/download/public/client.h"
#include "components/download/public/clients.h"
#include "components/download/public/download_params.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download {

// An entry in the Model that represents a scheduled download.
struct Entry {
 public:
  enum class State {
    // A newly added download.  The Entry is not guaranteed to be persisted in
    // the model yet.
    NEW = 0,

    // The download has been persisted and is available to start, pending
    // scheduler criteria.
    AVAILABLE = 1,

    // The download is active.  The DownloadDriver is aware of it and it is
    // either being downloaded or suspended by the scheduler due to device
    // characteristics or throttling.
    ACTIVE = 2,

    // The download has been paused by the owning Client.  The download will not
    // be run until it is resumed by the Client.
    PAUSED = 3,

    // The download is 'complete' and successful.  At this point we are leaving
    // this entry around to make sure the files on disk are cleaned up.
    COMPLETE = 4,

    // The count of entries for the enum.
    COUNT = 5,
  };

  Entry();
  Entry(const Entry& other);
  explicit Entry(const DownloadParams& params);
  ~Entry();

  bool operator==(const Entry& other) const;

  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // The feature that is requesting this download.
  DownloadClient client = DownloadClient::INVALID;

  // A unique GUID that represents this download.  See | base::GenerateGUID()|.
  std::string guid;

  // The time when the entry is created.
  base::Time create_time;

  // The parameters that determine under what device conditions this download
  // will occur.
  SchedulingParams scheduling_params;

  // The parameters that define the actual download request to make.
  RequestParams request_params;

  // The state of the download to help the scheduler and loggers make the right
  // decisions about the download object.
  State state = State::NEW;

  // Target file path for this download.
  base::FilePath target_file_path;

  // Time the download was marked as complete, base::Time() if the download is
  // not yet complete.
  base::Time completion_time;

  // Last time when the entry was checked for cleanup, default is
  // |completion_time|.
  base::Time last_cleanup_check_time;

  // Size of the download file in bytes, 0 if download is not successfully
  // completed.
  uint64_t bytes_downloaded;

  // Stores the number of retries for this download.
  uint32_t attempt_count;

  // Stores the number of resumptions for this download.
  uint32_t resumption_count;

  // Stores the number of times the service tried to delete the download file.
  uint32_t cleanup_attempt_count;

  // Traffic annotation for the network request.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_H_
