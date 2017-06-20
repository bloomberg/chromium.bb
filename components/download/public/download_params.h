// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_DOWNLOAD_PARAMS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_DOWNLOAD_PARAMS_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "components/download/public/clients.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace download {

// The parameters describing when to run a download.  This allows the caller to
// specify restrictions on what impact this download will have on the device
// (battery, network conditions, priority, etc.).
struct SchedulingParams {
 public:
  enum class NetworkRequirements {
    // The download can occur under all network conditions.
    NONE = 0,

    // The download should occur when the network isn't metered.  However if the
    // device does not provide that opportunity over a long period of time, the
    // DownloadService may start allowing these downloads to run on metered
    // networks as well.
    OPTIMISTIC = 1,

    // The download can occur only if the network isn't metered.
    UNMETERED = 2,
  };

  enum class BatteryRequirements {
    // The download can occur under all battery scenarios.  Note that the
    // DownloadService may still not run this download under extremely low
    // battery conditions.
    BATTERY_INSENSITIVE = 0,

    // The download can only occur when charging or in optimal battery
    // conditions.
    BATTERY_SENSITIVE = 1,
  };

  enum class Priority {
    // The lowest priority.  Requires that the device is idle or Chrome is
    // running.
    LOW = 0,

    // The normal priority.  Requires that the device is idle or Chrome is
    // running.
    NORMAL = 1,

    // The highest background priority.  Does not require the device to be idle.
    HIGH = 2,

    // The highest priority.  This will act (scheduling requirements aside) as a
    // user-initiated download.
    UI = 3,

    // The default priority for all tasks unless overridden.
    DEFAULT = NORMAL,
  };

  SchedulingParams();
  SchedulingParams(const SchedulingParams& other) = default;
  ~SchedulingParams() = default;

  bool operator==(const SchedulingParams& rhs) const;

  // Cancel the download after this time.  Will cancel in-progress downloads.
  base::Time cancel_time;

  // The suggested priority.  Non-UI priorities may not be honored by the
  // DownloadService based on internal criteria and settings.
  Priority priority;
  NetworkRequirements network_requirements;
  BatteryRequirements battery_requirements;
};

// The parameters describing how to build the request when starting a download.
struct RequestParams {
 public:
  RequestParams();
  RequestParams(const RequestParams& other) = default;
  ~RequestParams() = default;

  GURL url;

  // The request method ("GET" is the default value).
  std::string method;
  net::HttpRequestHeaders request_headers;
};

// The parameters that describe a download request made to the DownloadService.
// The |client| needs to be properly created and registered for this service for
// the download to be accepted.
struct DownloadParams {
  enum StartResult {
    // The download is accepted and persisted.
    ACCEPTED,

    // The DownloadService has too many downloads.  Backoff and retry.
    BACKOFF,

    // The DownloadService has no knowledge of the DownloadClient associated
    // with this request.
    UNEXPECTED_CLIENT,

    // Failed to create the download.  The guid is already in use.
    UNEXPECTED_GUID,

    // The download was cancelled by the Client while it was being persisted.
    CLIENT_CANCELLED,

    // The DownloadService was unable to accept and persist this download due to
    // an internal error like the underlying DB store failing to write to disk.
    INTERNAL_ERROR,

    // TODO(dtrainor): Add more error codes.
  };

  using StartCallback = base::Callback<void(const std::string&, StartResult)>;

  DownloadParams();
  DownloadParams(const DownloadParams& other);
  ~DownloadParams();

  // The feature that is requesting this download.
  DownloadClient client;

  // A unique GUID that represents this download.  See |base::GenerateGUID()|.
  // TODO(xingliu): guid in content download must be upper case, see
  // http://crbug.com/734818.
  std::string guid;

  // A callback that will be notified if this download has been accepted and
  // persisted by the DownloadService.
  StartCallback callback;

  // The parameters that determine under what device conditions this download
  // will occur.
  SchedulingParams scheduling_params;

  // The parameters that define the actual download request to make.
  RequestParams request_params;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_DOWNLOAD_PARAMS_H_
