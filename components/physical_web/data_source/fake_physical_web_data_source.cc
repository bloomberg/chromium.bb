// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/data_source/fake_physical_web_data_source.h"

#include "base/strings/string_number_conversions.h"
#include "components/physical_web/data_source/physical_web_listener.h"
#include "url/gurl.h"

namespace physical_web {

std::unique_ptr<Metadata> CreatePhysicalWebPage(
    const std::string& resolved_url,
    double distance_estimate,
    const std::string& group_id,
    int scan_timestamp,
    const std::string& title,
    const std::string& description,
    const std::string& scanned_url) {
  auto page = base::MakeUnique<Metadata>();
  page->resolved_url = GURL(resolved_url);
  page->distance_estimate = distance_estimate;
  page->group_id = group_id;
  page->scan_timestamp = base::Time::FromJavaTime(scan_timestamp);
  page->title = title;
  page->description = description;
  page->scanned_url = GURL(scanned_url);
  return page;
}

std::unique_ptr<Metadata> CreateDummyPhysicalWebPage(int id,
                                                     double distance,
                                                     int timestamp) {
  const std::string id_string = base::IntToString(id);
  return CreatePhysicalWebPage("https://resolved_url.com/" + id_string,
                               distance, /*group_id=*/std::string(), timestamp,
                               "title " + id_string, "description " + id_string,
                               "https://scanned_url.com/" + id_string);
}

std::unique_ptr<MetadataList> CreateDummyPhysicalWebPages(
    const std::vector<int>& ids) {
  int distance = 1;
  int timestamp = static_cast<int>(ids.size());
  auto list = base::MakeUnique<MetadataList>();
  for (int id : ids) {
    std::unique_ptr<Metadata> page =
        CreateDummyPhysicalWebPage(id, distance, timestamp);
    list->push_back(*page);
    ++distance;
    --timestamp;
  }
  return list;
}

FakePhysicalWebDataSource::FakePhysicalWebDataSource() {}

FakePhysicalWebDataSource::~FakePhysicalWebDataSource() = default;

void FakePhysicalWebDataSource::StartDiscovery(bool network_request_enabled) {
  // Ignored.
}

void FakePhysicalWebDataSource::StopDiscovery() {
  // Ignored.
}

std::unique_ptr<MetadataList> FakePhysicalWebDataSource::GetMetadataList() {
  return base::MakeUnique<MetadataList>(*metadata_list_.get());
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

void FakePhysicalWebDataSource::SetMetadataList(
    std::unique_ptr<MetadataList> metadata_list) {
  metadata_list_ = std::move(metadata_list);
}

void FakePhysicalWebDataSource::NotifyOnFound(const GURL& url) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnFound(url);
}

void FakePhysicalWebDataSource::NotifyOnLost(const GURL& url) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnLost(url);
}

void FakePhysicalWebDataSource::NotifyOnDistanceChanged(
    const GURL& url,
    double distance_estimate) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnDistanceChanged(url, distance_estimate);
}

}  // namespace physical_web
