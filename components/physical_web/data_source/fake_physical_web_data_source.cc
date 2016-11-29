// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/data_source/fake_physical_web_data_source.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/physical_web/data_source/physical_web_listener.h"

using base::ListValue;
using base::DictionaryValue;

namespace physical_web {

std::unique_ptr<DictionaryValue> CreatePhysicalWebPage(
    const std::string& resolved_url,
    double distance_estimate,
    int scan_timestamp,
    const std::string& title,
    const std::string& description,
    const std::string& scanned_url) {
  auto page = base::MakeUnique<DictionaryValue>();
  page->SetString(kScannedUrlKey, scanned_url);
  page->SetDouble(kDistanceEstimateKey, distance_estimate);
  // TODO(crbug.com/667722): Remove this integer workaround once timestamp is
  // fixed.
  page->SetInteger(kScanTimestampKey, scan_timestamp);
  page->SetString(kResolvedUrlKey, resolved_url);
  page->SetString(kTitleKey, title);
  page->SetString(kDescriptionKey, description);
  return page;
}

std::unique_ptr<DictionaryValue> CreateDummyPhysicalWebPage(int id,
                                                            double distance,
                                                            int timestamp) {
  const std::string id_string = base::IntToString(id);
  return CreatePhysicalWebPage("https://resolved_url.com/" + id_string,
                               distance, timestamp, "title " + id_string,
                               "description " + id_string,
                               "https://scanned_url.com/" + id_string);
}

std::unique_ptr<ListValue> CreateDummyPhysicalWebPages(
    const std::vector<int>& ids) {
  int distance = 1;
  int timestamp = static_cast<int>(ids.size());
  auto list = base::MakeUnique<ListValue>();
  for (int id : ids) {
    list->Append(CreateDummyPhysicalWebPage(id, distance, timestamp));
    ++distance;
    --timestamp;
  }
  return list;
}

FakePhysicalWebDataSource::FakePhysicalWebDataSource()
    : metadata_(base::MakeUnique<ListValue>()) {}

FakePhysicalWebDataSource::~FakePhysicalWebDataSource() = default;

void FakePhysicalWebDataSource::StartDiscovery(bool network_request_enabled) {
  // Ignored.
}

void FakePhysicalWebDataSource::StopDiscovery() {
  // Ignored.
}

std::unique_ptr<base::ListValue> FakePhysicalWebDataSource::GetMetadata() {
  return metadata_->CreateDeepCopy();
}

bool FakePhysicalWebDataSource::HasUnresolvedDiscoveries() {
  return false;
}

void FakePhysicalWebDataSource::RegisterListener(
    PhysicalWebListener* physical_web_listener) {
  observer_list_.AddObserver(physical_web_listener);
}

void FakePhysicalWebDataSource::UnregisterListener(
    PhysicalWebListener* physical_web_listener) {
  observer_list_.RemoveObserver(physical_web_listener);
}

void FakePhysicalWebDataSource::SetMetadata(
    std::unique_ptr<ListValue> metadata) {
  metadata_ = std::move(metadata);
}

void FakePhysicalWebDataSource::NotifyOnFound(const std::string& url) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnFound(url);
}

void FakePhysicalWebDataSource::NotifyOnLost(const std::string& url) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnLost(url);
}

void FakePhysicalWebDataSource::NotifyOnDistanceChanged(
    const std::string& url,
    double distance_estimate) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnDistanceChanged(url, distance_estimate);
}

}  // namespace physical_web
