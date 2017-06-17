// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_DOWNLOAD_DRIVER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_DOWNLOAD_DRIVER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "components/download/internal/download_driver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download {
namespace test {

// Download driver that simulates content layer download logic.
class TestDownloadDriver : public DownloadDriver {
 public:
  TestDownloadDriver();
  ~TestDownloadDriver() override;

  // Marks download driver as ready, used to test logic that depends on
  // data initialization.
  void MakeReady();

  // Adds driver entries data that will be returned
  void AddTestData(const std::vector<DriverEntry>& entries);

  // Simulates download events from content layer.
  void NotifyDownloadUpdate(const DriverEntry& entry);
  void NotifyDownloadFailed(const DriverEntry& entry, int reason);
  void NotifyDownloadSucceeded(const DriverEntry& entry,
                               const base::FilePath& path);

  // DownloadDriver implementation.
  void Initialize(DownloadDriver::Client* client) override;
  bool IsReady() const override;
  void Start(
      const RequestParams& params,
      const std::string& guid,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  void Remove(const std::string& guid) override;
  void Pause(const std::string& guid) override;
  void Resume(const std::string& guid) override;
  base::Optional<DriverEntry> Find(const std::string& guid) override;
  std::set<std::string> GetActiveDownloads() override;

 private:
  bool is_ready_;
  DownloadDriver::Client* client_;

  // Map of guid --> DriverEntry.
  std::map<std::string, DriverEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadDriver);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_TEST_TEST_DOWNLOAD_DRIVER_H_
