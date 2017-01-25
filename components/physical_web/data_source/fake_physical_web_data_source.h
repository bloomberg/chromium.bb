// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_FAKE_PHYSICAL_WEB_DATA_SOURCE_H_
#define COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_FAKE_PHYSICAL_WEB_DATA_SOURCE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

class GURL;

namespace physical_web {

std::unique_ptr<Metadata> CreatePhysicalWebPage(
    const std::string& resolved_url,
    double distance_estimate,
    const std::string& group_id,
    int scan_timestamp,
    const std::string& title,
    const std::string& description,
    const std::string& scanned_url);

std::unique_ptr<Metadata>
CreateDummyPhysicalWebPage(int id, double distance, int timestamp);

std::unique_ptr<MetadataList> CreateDummyPhysicalWebPages(
    const std::vector<int>& ids);

class FakePhysicalWebDataSource : public PhysicalWebDataSource {
 public:
  FakePhysicalWebDataSource();
  ~FakePhysicalWebDataSource() override;

  void StartDiscovery(bool network_request_enabled) override;
  void StopDiscovery() override;

  std::unique_ptr<MetadataList> GetMetadataList() override;

  bool HasUnresolvedDiscoveries() override;

  void RegisterListener(PhysicalWebListener* physical_web_listener) override;
  void UnregisterListener(PhysicalWebListener* physical_web_listener) override;

  // for testing
  void SetMetadataList(std::unique_ptr<MetadataList> metadata_list);
  void NotifyOnFound(const GURL& url);
  void NotifyOnLost(const GURL& url);
  void NotifyOnDistanceChanged(const GURL& url, double distance_estimate);

 private:
  std::unique_ptr<MetadataList> metadata_list_;
  base::ObserverList<PhysicalWebListener> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakePhysicalWebDataSource);
};

}  // namespace physical_web

#endif  // COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_FAKE_PHYSICAL_WEB_DATA_SOURCE_H_
