// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_item.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

PrefetchItem::PrefetchItem() = default;

PrefetchItem::PrefetchItem(const PrefetchItem& other) = default;

PrefetchItem::PrefetchItem(PrefetchItem&& other) = default;

PrefetchItem::~PrefetchItem() = default;

PrefetchItem& PrefetchItem::operator=(const PrefetchItem& other) = default;

PrefetchItem& PrefetchItem::operator=(PrefetchItem&& other) = default;

bool PrefetchItem::operator==(const PrefetchItem& other) const {
  return offline_id == other.offline_id && guid == other.guid &&
         client_id == other.client_id && state == other.state &&
         url == other.url && final_archived_url == other.final_archived_url &&
         generate_bundle_attempts == other.generate_bundle_attempts &&
         get_operation_attempts == other.get_operation_attempts &&
         download_initiation_attempts == other.download_initiation_attempts &&
         operation_name == other.operation_name &&
         archive_body_name == other.archive_body_name &&
         archive_body_length == other.archive_body_length &&
         creation_time == other.creation_time &&
         freshness_time == other.freshness_time &&
         error_code == other.error_code && title == other.title &&
         file_path == other.file_path && file_size == other.file_size;
}

bool PrefetchItem::operator!=(const PrefetchItem& other) const {
  return !(*this == other);
}

bool PrefetchItem::operator<(const PrefetchItem& other) const {
  return offline_id < other.offline_id;
}

std::string PrefetchItem::ToString() const {
  std::string s("PrefetchItem(");
  s.append(base::Int64ToString(offline_id)).append(", ");
  s.append(guid).append(", ");
  s.append(client_id.ToString()).append(", ");
  s.append(base::IntToString(static_cast<int>(state))).append(", ");
  s.append(url.possibly_invalid_spec()).append(", ");
  s.append(final_archived_url.possibly_invalid_spec()).append(", ");
  s.append(base::IntToString(generate_bundle_attempts)).append(", ");
  s.append(base::IntToString(get_operation_attempts)).append(", ");
  s.append(base::IntToString(download_initiation_attempts)).append(", ");
  s.append(operation_name).append(", ");
  s.append(archive_body_name).append(", ");
  s.append(base::IntToString(archive_body_length)).append(", ");
  s.append(base::Int64ToString(store_utils::ToDatabaseTime(creation_time)))
      .append(", ");
  s.append(base::Int64ToString(store_utils::ToDatabaseTime(freshness_time)))
      .append(", ");
  s.append(base::IntToString(static_cast<int>(error_code))).append(", ");
  s.append(base::UTF16ToUTF8(title)).append(", ");
  s.append(file_path.AsUTF8Unsafe()).append(", ");
  s.append(base::IntToString(static_cast<int>(file_size))).append(")");
  return s;
}

std::ostream& operator<<(std::ostream& out, const PrefetchItem& pi) {
  return out << pi.ToString();
}

}  // namespace offline_pages
