// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/download_params.h"

#include "components/download/public/clients.h"

namespace download {

SchedulingParams::SchedulingParams()
    : priority(Priority::DEFAULT),
      network_requirements(NetworkRequirements::NONE),
      battery_requirements(BatteryRequirements::BATTERY_INSENSITIVE) {}

RequestParams::RequestParams() : method("GET") {}

DownloadParams::DownloadParams() : client(DownloadClient::INVALID) {}

DownloadParams::DownloadParams(const DownloadParams& other) = default;

DownloadParams::~DownloadParams() = default;

}  // namespace download
