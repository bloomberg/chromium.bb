// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/entry.h"

namespace download {

Entry::Entry() = default;
Entry::Entry(const Entry& other) = default;

Entry::Entry(const DownloadParams& params)
    : client(params.client),
      guid(params.guid),
      create_time(base::Time::Now()),
      scheduling_params(params.scheduling_params),
      request_params(params.request_params) {}

Entry::~Entry() = default;

bool Entry::operator==(const Entry& other) const {
  return client == other.client && guid == other.guid &&
         scheduling_params.cancel_time == other.scheduling_params.cancel_time &&
         scheduling_params.network_requirements ==
             other.scheduling_params.network_requirements &&
         scheduling_params.battery_requirements ==
             other.scheduling_params.battery_requirements &&
         scheduling_params.priority == other.scheduling_params.priority &&
         request_params.url == other.request_params.url &&
         request_params.method == other.request_params.method &&
         request_params.request_headers.ToString() ==
             other.request_params.request_headers.ToString() &&
         state == other.state && target_file_path == other.target_file_path &&
         create_time == other.create_time &&
         completion_time == other.completion_time;
}

}  // namespace download
