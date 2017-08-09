// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

namespace offline_pages {

// All static.
const std::string MockPrefetchItemGenerator::kClientNamespace(
    "test_prefetch_namespace");
const std::string MockPrefetchItemGenerator::kClientIdPrefix("test_client_id_");
const std::string MockPrefetchItemGenerator::kUrlPrefix(
    "http://www.requested.com/");
const std::string MockPrefetchItemGenerator::kFinalUrlPrefix(
    "http://www.final.com/");
const std::string MockPrefetchItemGenerator::kOperationNamePrefix(
    "test_operation_name_");
const std::string MockPrefetchItemGenerator::kArchiveBodyNamePrefix(
    "test_archive_body_name_");

MockPrefetchItemGenerator::MockPrefetchItemGenerator() = default;

MockPrefetchItemGenerator::~MockPrefetchItemGenerator() = default;

PrefetchItem MockPrefetchItemGenerator::CreateItem(PrefetchItemState state) {
  static int item_counter = 0;
  PrefetchItem new_item;

  // Values set with non prefix based values.
  new_item.state = state;
  new_item.guid = base::GenerateGUID();
  new_item.offline_id = GenerateTestOfflineId();
  new_item.creation_time = base::Time::Now();
  new_item.freshness_time = new_item.creation_time;

  // Values always set using prefixes.
  new_item.client_id = ClientId(
      client_namespace_, client_id_prefix_ + base::IntToString(item_counter));
  new_item.url = GURL(url_prefix_ + base::IntToString(item_counter));

  // Values set only if prefixes are not empty.
  if (final_url_prefix_.length())
    new_item.final_archived_url =
        GURL(final_url_prefix_ + base::IntToString(item_counter));
  if (operation_name_prefix_.length())
    new_item.operation_name =
        operation_name_prefix_ + base::IntToString(item_counter);
  if (archive_body_name_prefix_.length()) {
    new_item.archive_body_name =
        archive_body_name_prefix_ + base::IntToString(item_counter);
    new_item.archive_body_length = item_counter * 100;
  }

  ++item_counter;
  return new_item;
}

int64_t MockPrefetchItemGenerator::GenerateTestOfflineId() {
  return ++offline_id_counter_;
}

}  // namespace offline_pages
