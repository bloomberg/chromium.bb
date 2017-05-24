// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/test/test_download_driver.h"

#include "base/files/file_path.h"
#include "components/download/public/download_params.h"
#include "net/http/http_response_headers.h"

namespace download {

TestDownloadDriver::TestDownloadDriver() : is_ready_(false), client_(nullptr) {}

TestDownloadDriver::~TestDownloadDriver() = default;

void TestDownloadDriver::MakeReady() {
  is_ready_ = true;
  DCHECK(client_);
  if (client_)
    client_->OnDriverReady(is_ready_);
}

void TestDownloadDriver::NotifyDownloadUpdate(const DriverEntry& entry) {
  if (client_) {
    entries_[entry.guid] = entry;
    client_->OnDownloadUpdated(entry);
  }
}

void TestDownloadDriver::NotifyDownloadFailed(const DriverEntry& entry,
                                              int reason) {
  if (client_) {
    entries_[entry.guid] = entry;
    client_->OnDownloadFailed(entry, reason);
  }
}

void TestDownloadDriver::NotifyDownloadSucceeded(const DriverEntry& entry,
                                                 const base::FilePath& path) {
  if (client_) {
    entries_[entry.guid] = entry;
    client_->OnDownloadSucceeded(entry, path);
  }
}

void TestDownloadDriver::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_);
  client_ = client;
}

bool TestDownloadDriver::IsReady() const {
  return is_ready_;
}

void TestDownloadDriver::Start(const DownloadParams& params) {
  DriverEntry entry;
  entry.guid = params.guid;
  entry.state = DriverEntry::State::IN_PROGRESS;
  entry.paused = false;
  entry.bytes_downloaded = 0;
  entry.expected_total_size = 0;
  entry.response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200");
  entries_[params.guid] = entry;

  if (client_)
    client_->OnDownloadCreated(entry);
}

void TestDownloadDriver::Cancel(const std::string& guid) {}

void TestDownloadDriver::Pause(const std::string& guid) {}

void TestDownloadDriver::Resume(const std::string& guid) {}

base::Optional<DriverEntry> TestDownloadDriver::Find(const std::string& guid) {
  auto it = entries_.find(guid);
  if (it == entries_.end())
    return base::nullopt;
  return it->second;
}

}  // namespace downloads
