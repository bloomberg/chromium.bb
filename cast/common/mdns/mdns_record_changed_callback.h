// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RECORD_CHANGED_CALLBACK_H_
#define CAST_COMMON_MDNS_MDNS_RECORD_CHANGED_CALLBACK_H_

namespace cast {
namespace mdns {

class MdnsRecord;

enum class RecordChangedEvent {
  kCreated,
  kUpdated,
  kDeleted,
};

class MdnsRecordChangedCallback {
 public:
  virtual ~MdnsRecordChangedCallback() = default;
  virtual void OnRecordChanged(const MdnsRecord& record,
                               RecordChangedEvent event) = 0;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RECORD_CHANGED_CALLBACK_H_