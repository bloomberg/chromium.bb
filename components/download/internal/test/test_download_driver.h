// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_DOWNLOAD_DRIVER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_DOWNLOAD_DRIVER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "components/download/internal/download_driver.h"

namespace download {

// Download driver that simulates content layer download logic.
class TestDownloadDriver : public DownloadDriver {
 public:
  TestDownloadDriver();
  ~TestDownloadDriver();

  // Marks download driver as ready, used to test logic that depends on
  // data initialization.
  void MakeReady();

  // Simulates download events from content layer.
  void NotifyDownloadUpdate(const DriverEntry& entry);
  void NotifyDownloadFailed(const DriverEntry& entry, int reason);
  void NotifyDownloadSucceeded(const DriverEntry& entry,
                               const base::FilePath& path);

  // DownloadDriver implementation.
  void Initialize(DownloadDriver::Client* client) override;
  bool IsReady() const override;
  void Start(const DownloadParams& params) override;
  void Cancel(const std::string& guid) override;
  void Pause(const std::string& guid) override;
  void Resume(const std::string& guid) override;
  base::Optional<DriverEntry> Find(const std::string& guid) override;

 private:
  bool is_ready_;
  DownloadDriver::Client* client_;

  // Map of guid --> DriverEntry.
  std::map<std::string, DriverEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadDriver);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_TEST_TEST_DOWNLOAD_DRIVER_H_
