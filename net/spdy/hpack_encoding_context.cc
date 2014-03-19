// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoding_context.h"

#include <cstddef>

#include "base/logging.h"
#include "base/macros.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_entry.h"

namespace net {

using base::StringPiece;

namespace {

// An entry in the static table. Must be a POD in order to avoid
// static initializers, i.e. no user-defined constructors or
// destructors.
struct StaticEntry {
  const char* const name;
  const size_t name_len;
  const char* const value;
  const size_t value_len;
};

// The "constructor" for a StaticEntry that computes the lengths at
// compile time.
#define STATIC_ENTRY(name, value) \
  { name, arraysize(name) - 1, value, arraysize(value) - 1 }

const StaticEntry kStaticTable[] = {
  STATIC_ENTRY(":authority"                  , ""),             // 1
  STATIC_ENTRY(":method"                     , "GET"),          // 2
  STATIC_ENTRY(":method"                     , "POST"),         // 3
  STATIC_ENTRY(":path"                       , "/"),            // 4
  STATIC_ENTRY(":path"                       , "/index.html"),  // 5
  STATIC_ENTRY(":scheme"                     , "http"),         // 6
  STATIC_ENTRY(":scheme"                     , "https"),        // 7
  STATIC_ENTRY(":status"                     , "200"),          // 8
  STATIC_ENTRY(":status"                     , "500"),          // 9
  STATIC_ENTRY(":status"                     , "404"),          // 10
  STATIC_ENTRY(":status"                     , "403"),          // 11
  STATIC_ENTRY(":status"                     , "400"),          // 12
  STATIC_ENTRY(":status"                     , "401"),          // 13
  STATIC_ENTRY("accept-charset"              , ""),             // 14
  STATIC_ENTRY("accept-encoding"             , ""),             // 15
  STATIC_ENTRY("accept-language"             , ""),             // 16
  STATIC_ENTRY("accept-ranges"               , ""),             // 17
  STATIC_ENTRY("accept"                      , ""),             // 18
  STATIC_ENTRY("access-control-allow-origin" , ""),             // 19
  STATIC_ENTRY("age"                         , ""),             // 20
  STATIC_ENTRY("allow"                       , ""),             // 21
  STATIC_ENTRY("authorization"               , ""),             // 22
  STATIC_ENTRY("cache-control"               , ""),             // 23
  STATIC_ENTRY("content-disposition"         , ""),             // 24
  STATIC_ENTRY("content-encoding"            , ""),             // 25
  STATIC_ENTRY("content-language"            , ""),             // 26
  STATIC_ENTRY("content-length"              , ""),             // 27
  STATIC_ENTRY("content-location"            , ""),             // 28
  STATIC_ENTRY("content-range"               , ""),             // 29
  STATIC_ENTRY("content-type"                , ""),             // 30
  STATIC_ENTRY("cookie"                      , ""),             // 31
  STATIC_ENTRY("date"                        , ""),             // 32
  STATIC_ENTRY("etag"                        , ""),             // 33
  STATIC_ENTRY("expect"                      , ""),             // 34
  STATIC_ENTRY("expires"                     , ""),             // 35
  STATIC_ENTRY("from"                        , ""),             // 36
  STATIC_ENTRY("host"                        , ""),             // 37
  STATIC_ENTRY("if-match"                    , ""),             // 38
  STATIC_ENTRY("if-modified-since"           , ""),             // 39
  STATIC_ENTRY("if-none-match"               , ""),             // 40
  STATIC_ENTRY("if-range"                    , ""),             // 41
  STATIC_ENTRY("if-unmodified-since"         , ""),             // 42
  STATIC_ENTRY("last-modified"               , ""),             // 43
  STATIC_ENTRY("link"                        , ""),             // 44
  STATIC_ENTRY("location"                    , ""),             // 45
  STATIC_ENTRY("max-forwards"                , ""),             // 46
  STATIC_ENTRY("proxy-authenticate"          , ""),             // 47
  STATIC_ENTRY("proxy-authorization"         , ""),             // 48
  STATIC_ENTRY("range"                       , ""),             // 49
  STATIC_ENTRY("referer"                     , ""),             // 50
  STATIC_ENTRY("refresh"                     , ""),             // 51
  STATIC_ENTRY("retry-after"                 , ""),             // 52
  STATIC_ENTRY("server"                      , ""),             // 53
  STATIC_ENTRY("set-cookie"                  , ""),             // 54
  STATIC_ENTRY("strict-transport-security"   , ""),             // 55
  STATIC_ENTRY("transfer-encoding"           , ""),             // 56
  STATIC_ENTRY("user-agent"                  , ""),             // 57
  STATIC_ENTRY("vary"                        , ""),             // 58
  STATIC_ENTRY("via"                         , ""),             // 59
  STATIC_ENTRY("www-authenticate"            , ""),             // 60
};

#undef STATIC_ENTRY

const size_t kStaticEntryCount = arraysize(kStaticTable);

}  // namespace

// Must match HpackEntry::kUntouched.
const uint32 HpackEncodingContext::kUntouched = 0x7fffffff;

HpackEncodingContext::HpackEncodingContext()
    : settings_header_table_size_(kDefaultHeaderTableSizeSetting) {
  DCHECK_EQ(HpackEncodingContext::kUntouched, HpackEntry::kUntouched);
}

HpackEncodingContext::~HpackEncodingContext() {}

uint32 HpackEncodingContext::GetMutableEntryCount() const {
  return header_table_.GetEntryCount();
}

uint32 HpackEncodingContext::GetEntryCount() const {
  return GetMutableEntryCount() + kStaticEntryCount;
}

StringPiece HpackEncodingContext::GetNameAt(uint32 index) const {
  CHECK_GE(index, 1u);
  CHECK_LE(index, GetEntryCount());
  if (index > header_table_.GetEntryCount()) {
    const StaticEntry& entry =
        kStaticTable[index - header_table_.GetEntryCount() - 1];
    return StringPiece(entry.name, entry.name_len);
  }
  return header_table_.GetEntry(index).name();
}

StringPiece HpackEncodingContext::GetValueAt(uint32 index) const {
  CHECK_GE(index, 1u);
  CHECK_LE(index, GetEntryCount());
  if (index > header_table_.GetEntryCount()) {
    const StaticEntry& entry =
        kStaticTable[index - header_table_.GetEntryCount() - 1];
    return StringPiece(entry.value, entry.value_len);
  }
  return header_table_.GetEntry(index).value();
}

bool HpackEncodingContext::IsReferencedAt(uint32 index) const {
  CHECK_GE(index, 1u);
  CHECK_LE(index, GetEntryCount());
  if (index > header_table_.GetEntryCount())
    return false;
  return header_table_.GetEntry(index).IsReferenced();
}

uint32 HpackEncodingContext::GetTouchCountAt(uint32 index) const {
  CHECK_GE(index, 1u);
  CHECK_LE(index, GetEntryCount());
  if (index > header_table_.GetEntryCount())
    return 0;
  return header_table_.GetEntry(index).TouchCount();
}

void HpackEncodingContext::SetReferencedAt(uint32 index, bool referenced) {
  header_table_.GetMutableEntry(index)->SetReferenced(referenced);
}

void HpackEncodingContext::AddTouchesAt(uint32 index, uint32 touch_count) {
  header_table_.GetMutableEntry(index)->AddTouches(touch_count);
}

void HpackEncodingContext::ClearTouchesAt(uint32 index) {
  header_table_.GetMutableEntry(index)->ClearTouches();
}

void HpackEncodingContext::ApplyHeaderTableSizeSetting(uint32 size) {
  settings_header_table_size_ = size;
  if (size < header_table_.max_size()) {
    // Implicit maximum-size context update.
    CHECK(ProcessContextUpdateNewMaximumSize(size));
  }
}

bool HpackEncodingContext::ProcessContextUpdateNewMaximumSize(uint32 size) {
  if (size > settings_header_table_size_) {
    return false;
  }
  header_table_.SetMaxSize(size);
  return true;
}

bool HpackEncodingContext::ProcessContextUpdateEmptyReferenceSet() {
  for (size_t i = 1; i <= header_table_.GetEntryCount(); ++i) {
    HpackEntry* entry = header_table_.GetMutableEntry(i);
    if (entry->IsReferenced()) {
      entry->SetReferenced(false);
    }
  }
  return true;
}

bool HpackEncodingContext::ProcessIndexedHeader(uint32 index, uint32* new_index,
    std::vector<uint32>* removed_referenced_indices) {
  CHECK_GT(index, 0u);
  CHECK_LT(index, GetEntryCount());

  if (index <= header_table_.GetEntryCount()) {
    *new_index = index;
    removed_referenced_indices->clear();
    HpackEntry* entry = header_table_.GetMutableEntry(index);
    entry->SetReferenced(!entry->IsReferenced());
  } else {
    // TODO(akalin): Make HpackEntry know about owned strings and
    // non-owned strings so that it can potentially avoid copies here.
    HpackEntry entry(GetNameAt(index), GetValueAt(index));

    header_table_.TryAddEntry(entry, new_index, removed_referenced_indices);
    if (*new_index >= 1) {
      header_table_.GetMutableEntry(*new_index)->SetReferenced(true);
    }
  }
  return true;
}

bool HpackEncodingContext::ProcessLiteralHeaderWithIncrementalIndexing(
    StringPiece name,
    StringPiece value,
    uint32* index,
    std::vector<uint32>* removed_referenced_indices) {
  HpackEntry entry(name, value);
  header_table_.TryAddEntry(entry, index, removed_referenced_indices);
  if (*index >= 1) {
    header_table_.GetMutableEntry(*index)->SetReferenced(true);
  }
  return true;
}

}  // namespace net
