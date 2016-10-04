// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/write_transaction_info.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"

namespace syncer {
namespace syncable {

WriteTransactionInfo::WriteTransactionInfo(
    int64_t id,
    tracked_objects::Location location,
    WriterTag writer,
    ImmutableEntryKernelMutationMap mutations)
    : id(id),
      location_string(location.ToString()),
      writer(writer),
      mutations(mutations) {}

WriteTransactionInfo::WriteTransactionInfo() : id(-1), writer(INVALID) {}

WriteTransactionInfo::WriteTransactionInfo(const WriteTransactionInfo& other) =
    default;

WriteTransactionInfo::~WriteTransactionInfo() {}

base::DictionaryValue* WriteTransactionInfo::ToValue(
    size_t max_mutations_size) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("id", base::Int64ToString(id));
  dict->SetString("location", location_string);
  dict->SetString("writer", WriterTagToString(writer));
  std::unique_ptr<base::Value> mutations_value;
  const size_t mutations_size = mutations.Get().size();
  if (mutations_size <= max_mutations_size) {
    mutations_value = EntryKernelMutationMapToValue(mutations.Get());
  } else {
    mutations_value = base::MakeUnique<base::StringValue>(
        base::SizeTToString(mutations_size) + " mutations");
  }
  dict->Set("mutations", std::move(mutations_value));
  return dict;
}

}  // namespace syncable
}  // namespace syncer
