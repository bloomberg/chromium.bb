// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_FAKE_PHYSICAL_WEB_DATA_SOURCE_H_
#define COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_FAKE_PHYSICAL_WEB_DATA_SOURCE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace physical_web {

std::unique_ptr<base::DictionaryValue> CreatePhysicalWebPage(
    const std::string& resolved_url,
    double distance_estimate,
    int scan_timestamp,
    const std::string& title,
    const std::string& description,
    const std::string& scanned_url);

std::unique_ptr<base::DictionaryValue>
CreateDummyPhysicalWebPage(int id, double distance, int timestamp);

std::unique_ptr<base::ListValue> CreateDummyPhysicalWebPages(
    const std::vector<int>& ids);

class FakePhysicalWebDataSource : public PhysicalWebDataSource {
 public:
  FakePhysicalWebDataSource();
  ~FakePhysicalWebDataSource() override;

  void StartDiscovery(bool network_request_enabled) override;
  void StopDiscovery() override;

  std::unique_ptr<base::ListValue> GetMetadata() override;

  bool HasUnresolvedDiscoveries() override;

  void RegisterListener(PhysicalWebListener* physical_web_listener) override;
  void UnregisterListener(PhysicalWebListener* physical_web_listener) override;

  // for testing
  void SetMetadata(std::unique_ptr<base::ListValue> metadata);
  void NotifyOnFound(const std::string& url);
  void NotifyOnLost(const std::string& url);
  void NotifyOnDistanceChanged(const std::string& url,
                               double distance_estimate);

 private:
  std::unique_ptr<base::ListValue> metadata_;
  base::ObserverList<PhysicalWebListener> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakePhysicalWebDataSource);
};

}  // namespace physical_web

#endif  // COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_FAKE_PHYSICAL_WEB_DATA_SOURCE_H_
