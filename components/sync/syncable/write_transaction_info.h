// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_WRITE_TRANSACTION_INFO_H_
#define COMPONENTS_SYNC_SYNCABLE_WRITE_TRANSACTION_INFO_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "components/sync/syncable/entry_kernel.h"
#include "components/sync/syncable/syncable_base_transaction.h"

namespace syncer {
namespace syncable {

// A struct describing the changes made during a transaction.
struct WriteTransactionInfo {
  WriteTransactionInfo(int64_t id,
                       tracked_objects::Location location,
                       WriterTag writer,
                       ImmutableEntryKernelMutationMap mutations);
  WriteTransactionInfo();
  WriteTransactionInfo(const WriteTransactionInfo& other);
  ~WriteTransactionInfo();

  // Caller owns the return value.
  base::DictionaryValue* ToValue(size_t max_mutations_size) const;

  int64_t id;
  // If tracked_objects::Location becomes assignable, we can use that
  // instead.
  std::string location_string;
  WriterTag writer;
  ImmutableEntryKernelMutationMap mutations;
};

using ImmutableWriteTransactionInfo = Immutable<WriteTransactionInfo>;

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_WRITE_TRANSACTION_INFO_H_
