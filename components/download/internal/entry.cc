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

}  // namespace download
