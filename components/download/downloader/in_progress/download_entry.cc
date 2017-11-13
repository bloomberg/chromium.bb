// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/download_entry.h"

namespace download {

DownloadEntry::DownloadEntry() = default;

DownloadEntry::DownloadEntry(const DownloadEntry& other) = default;

DownloadEntry::DownloadEntry(const std::string& guid,
                             const std::string& request_origin)
    : guid(guid), request_origin(request_origin){};

DownloadEntry::~DownloadEntry() = default;

}  // namespace download
