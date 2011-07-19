// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with syncable.h.

#include "chrome/browser/sync/syncable/syncable_enum_conversions.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace syncable {

// We can't tokenize expected_min/expected_max since it can be a
// general expression.
#define ASSERT_ENUM_BOUNDS(enum_min, enum_max, expected_min, expected_max) \
  COMPILE_ASSERT(static_cast<int>(enum_min) ==                          \
                 static_cast<int>(expected_min),                        \
                 enum_min##_not_expected_min);                          \
  COMPILE_ASSERT(static_cast<int>(enum_max) ==                          \
                 static_cast<int>(expected_max),                        \
                 enum_max##_not_expected_max);

#define ENUM_CASE(enum_value) case enum_value: return #enum_value

const char* GetMetahandleFieldString(MetahandleField metahandle_field) {
  ASSERT_ENUM_BOUNDS(META_HANDLE, META_HANDLE,
                     INT64_FIELDS_BEGIN, BASE_VERSION - 1);
  switch (metahandle_field) {
    ENUM_CASE(META_HANDLE);
  }
  NOTREACHED();
  return "";
}

const char* GetBaseVersionString(BaseVersion base_version) {
  ASSERT_ENUM_BOUNDS(BASE_VERSION, BASE_VERSION,
                     META_HANDLE + 1, SERVER_VERSION - 1);
  switch (base_version) {
    ENUM_CASE(BASE_VERSION);
  }
  NOTREACHED();
  return "";
}

const char* GetInt64FieldString(Int64Field int64_field) {
  ASSERT_ENUM_BOUNDS(SERVER_VERSION, LOCAL_EXTERNAL_ID,
                     BASE_VERSION + 1, INT64_FIELDS_END - 1);
  switch (int64_field) {
    ENUM_CASE(SERVER_VERSION);
    ENUM_CASE(MTIME);
    ENUM_CASE(SERVER_MTIME);
    ENUM_CASE(CTIME);
    ENUM_CASE(SERVER_CTIME);
    ENUM_CASE(SERVER_POSITION_IN_PARENT);
    ENUM_CASE(LOCAL_EXTERNAL_ID);
    case INT64_FIELDS_END: break;
  }
  NOTREACHED();
  return "";
}

const char* GetIdFieldString(IdField id_field) {
  ASSERT_ENUM_BOUNDS(ID, NEXT_ID,
                     ID_FIELDS_BEGIN, ID_FIELDS_END - 1);
  switch (id_field) {
    ENUM_CASE(ID);
    ENUM_CASE(PARENT_ID);
    ENUM_CASE(SERVER_PARENT_ID);
    ENUM_CASE(PREV_ID);
    ENUM_CASE(NEXT_ID);
    case ID_FIELDS_END: break;
  }
  NOTREACHED();
  return "";
}

const char* GetIndexedBitFieldString(IndexedBitField indexed_bit_field) {
  ASSERT_ENUM_BOUNDS(IS_UNSYNCED, IS_UNAPPLIED_UPDATE,
                     BIT_FIELDS_BEGIN, INDEXED_BIT_FIELDS_END - 1);
  switch (indexed_bit_field) {
    ENUM_CASE(IS_UNSYNCED);
    ENUM_CASE(IS_UNAPPLIED_UPDATE);
    case INDEXED_BIT_FIELDS_END: break;
  }
  NOTREACHED();
  return "";
}

const char* GetIsDelFieldString(IsDelField is_del_field) {
  ASSERT_ENUM_BOUNDS(IS_DEL, IS_DEL,
                     INDEXED_BIT_FIELDS_END, IS_DIR - 1);
  switch (is_del_field) {
    ENUM_CASE(IS_DEL);
  }
  NOTREACHED();
  return "";
}

const char* GetBitFieldString(BitField bit_field) {
  ASSERT_ENUM_BOUNDS(IS_DIR, SERVER_IS_DEL,
                     IS_DEL + 1, BIT_FIELDS_END - 1);
  switch (bit_field) {
    ENUM_CASE(IS_DIR);
    ENUM_CASE(SERVER_IS_DIR);
    ENUM_CASE(SERVER_IS_DEL);
    case BIT_FIELDS_END: break;
  }
  NOTREACHED();
  return "";
}

const char* GetStringFieldString(StringField string_field) {
  ASSERT_ENUM_BOUNDS(NON_UNIQUE_NAME, UNIQUE_CLIENT_TAG,
                     STRING_FIELDS_BEGIN, STRING_FIELDS_END - 1);
  switch (string_field) {
    ENUM_CASE(NON_UNIQUE_NAME);
    ENUM_CASE(SERVER_NON_UNIQUE_NAME);
    ENUM_CASE(UNIQUE_SERVER_TAG);
    ENUM_CASE(UNIQUE_CLIENT_TAG);
    case STRING_FIELDS_END: break;
  }
  NOTREACHED();
  return "";
}

const char* GetProtoFieldString(ProtoField proto_field) {
  ASSERT_ENUM_BOUNDS(SPECIFICS, SERVER_SPECIFICS,
                     PROTO_FIELDS_BEGIN, PROTO_FIELDS_END - 1);
  switch (proto_field) {
    ENUM_CASE(SPECIFICS);
    ENUM_CASE(SERVER_SPECIFICS);
    case PROTO_FIELDS_END: break;
  }
  NOTREACHED();
  return "";
}

const char* GetBitTempString(BitTemp bit_temp) {
  ASSERT_ENUM_BOUNDS(SYNCING, SYNCING,
                     BIT_TEMPS_BEGIN, BIT_TEMPS_END - 1);
  switch (bit_temp) {
    ENUM_CASE(SYNCING);
    case BIT_TEMPS_END: break;
  }
  NOTREACHED();
  return "";
}

#undef ENUM_CASE
#undef ASSERT_ENUM_BOUNDS

}  // namespace syncable
