// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_ENUM_CONVERSIONS_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_ENUM_CONVERSIONS_H_
#pragma once

// Keep this file in sync with syncable.h.

#include "chrome/browser/sync/syncable/syncable.h"

// Utility functions to get the string equivalent for some syncable
// enums.

namespace syncable {

// The returned strings (which don't have to be freed) are in ASCII.
// The result of passing in an invalid enum value is undefined.

const char* GetMetahandleFieldString(MetahandleField metahandle_field);

const char* GetBaseVersionString(BaseVersion base_version);

const char* GetInt64FieldString(Int64Field int64_field);

const char* GetIdFieldString(IdField id_field);

const char* GetIndexedBitFieldString(IndexedBitField indexed_bit_field);

const char* GetIsDelFieldString(IsDelField is_del_field);

const char* GetBitFieldString(BitField bit_field);

const char* GetStringFieldString(StringField string_field);

const char* GetProtoFieldString(ProtoField proto_field);

const char* GetBitTempString(BitTemp bit_temp);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_ENUM_CONVERSIONS_H_
