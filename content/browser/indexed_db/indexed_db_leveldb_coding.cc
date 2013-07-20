// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

#include <iterator>
#include <limits>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"

// LevelDB stores key/value pairs. Keys and values are strings of bytes,
// normally of type std::string.
//
// The keys in the backing store are variable-length tuples with different types
// of fields. Each key in the backing store starts with a ternary prefix:
// (database id, object store id, index id). For each, 0 is reserved for
// meta-data.
// The prefix makes sure that data for a specific database, object store, and
// index are grouped together. The locality is important for performance: common
// operations should only need a minimal number of seek operations. For example,
// all the meta-data for a database is grouped together so that reading that
// meta-data only requires one seek.
//
// Each key type has a class (in square brackets below) which knows how to
// encode, decode, and compare that key type.
//
// Global meta-data have keys with prefix (0,0,0), followed by a type byte:
//
//     <0, 0, 0, 0>                                           =>
// IndexedDB/LevelDB schema version [SchemaVersionKey]
//     <0, 0, 0, 1>                                           => The maximum
// database id ever allocated [MaxDatabaseIdKey]
//     <0, 0, 0, 2>                                           =>
// SerializedScriptValue version [DataVersionKey]
//     <0, 0, 0, 100, database id>                            => Existence
// implies the database id is in the free list [DatabaseFreeListKey]
//     <0, 0, 0, 201, utf16 origin name, utf16 database name> => Database id
// [DatabaseNameKey]
//
//
// Database meta-data:
//
//     Again, the prefix is followed by a type byte.
//
//     <database id, 0, 0, 0> => utf16 origin name [DatabaseMetaDataKey]
//     <database id, 0, 0, 1> => utf16 database name [DatabaseMetaDataKey]
//     <database id, 0, 0, 2> => utf16 user version data [DatabaseMetaDataKey]
//     <database id, 0, 0, 3> => maximum object store id ever allocated
// [DatabaseMetaDataKey]
//     <database id, 0, 0, 4> => user integer version (var int)
// [DatabaseMetaDataKey]
//
//
// Object store meta-data:
//
//     The prefix is followed by a type byte, then a variable-length integer,
// and then another type byte.
//
//     <database id, 0, 0, 50, object store id, 0> => utf16 object store name
// [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 1> => utf16 key path
// [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 2> => has auto increment
// [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 3> => is evictable
// [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 4> => last "version" number
// [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 5> => maximum index id ever
// allocated [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 6> => has key path (vs. null)
// [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 7> => key generator current
// number [ObjectStoreMetaDataKey]
//
//
// Index meta-data:
//
//     The prefix is followed by a type byte, then two variable-length integers,
// and then another type byte.
//
//     <database id, 0, 0, 100, object store id, index id, 0> => utf16 index
// name [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 1> => are index keys
// unique [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 2> => utf16 key path
// [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 3> => is index
// multi-entry [IndexMetaDataKey]
//
//
// Other object store and index meta-data:
//
//     The prefix is followed by a type byte. The object store and index id are
// variable length integers, the utf16 strings are variable length strings.
//
//     <database id, 0, 0, 150, object store id>                   => existence
// implies the object store id is in the free list [ObjectStoreFreeListKey]
//     <database id, 0, 0, 151, object store id, index id>         => existence
// implies the index id is in the free list [IndexFreeListKey]
//     <database id, 0, 0, 200, utf16 object store name>           => object
// store id [ObjectStoreNamesKey]
//     <database id, 0, 0, 201, object store id, utf16 index name> => index id
// [IndexNamesKey]
//
//
// Object store data:
//
//     The prefix is followed by a type byte. The user key is an encoded
// IndexedDBKey.
//
//     <database id, object store id, 1, user key> => "version", serialized
// script value [ObjectStoreDataKey]
//
//
// "Exists" entry:
//
//     The prefix is followed by a type byte. The user key is an encoded
// IndexedDBKey.
//
//     <database id, object store id, 2, user key> => "version" [ExistsEntryKey]
//
//
// Index data:
//
//     The prefix is followed by a type byte. The index key is an encoded
// IndexedDBKey. The sequence number is a variable length integer.
//     The primary key is an encoded IndexedDBKey.
//
//     <database id, object store id, index id, index key, sequence number,
// primary key> => "version", primary key [IndexDataKey]
//
//     (The sequence number is obsolete; it was used to allow two entries with
//     the same user (index) key in non-unique indexes prior to the inclusion of
//     the primary key in the data. The "version" field is used to weed out
// stale
//     index data. Whenever new object store data is inserted, it gets a new
//     "version" number, and new index data is written with this number. When
//     the index is used for look-ups, entries are validated against the
//     "exists" entries, and records with old "version" numbers are deleted
//     when they are encountered in get_primary_key_via_index,
//     IndexCursorImpl::load_current_row, and
// IndexKeyCursorImpl::load_current_row).

using base::StringPiece;
using WebKit::WebIDBKeyType;
using WebKit::WebIDBKeyTypeArray;
using WebKit::WebIDBKeyTypeDate;
using WebKit::WebIDBKeyTypeInvalid;
using WebKit::WebIDBKeyTypeMin;
using WebKit::WebIDBKeyTypeNull;
using WebKit::WebIDBKeyTypeNumber;
using WebKit::WebIDBKeyTypeString;
using WebKit::WebIDBKeyPathType;
using WebKit::WebIDBKeyPathTypeArray;
using WebKit::WebIDBKeyPathTypeNull;
using WebKit::WebIDBKeyPathTypeString;

namespace content {

// As most of the IndexedDBKeys and encoded values are short, we
// initialize some Vectors with a default inline buffer size to reduce
// the memory re-allocations when the Vectors are appended.
static const size_t kDefaultInlineBufferSize = 32;

static const unsigned char kIndexedDBKeyNullTypeByte = 0;
static const unsigned char kIndexedDBKeyStringTypeByte = 1;
static const unsigned char kIndexedDBKeyDateTypeByte = 2;
static const unsigned char kIndexedDBKeyNumberTypeByte = 3;
static const unsigned char kIndexedDBKeyArrayTypeByte = 4;
static const unsigned char kIndexedDBKeyMinKeyTypeByte = 5;

static const unsigned char kIndexedDBKeyPathTypeCodedByte1 = 0;
static const unsigned char kIndexedDBKeyPathTypeCodedByte2 = 0;

static const unsigned char kObjectStoreDataIndexId = 1;
static const unsigned char kExistsEntryIndexId = 2;

static const unsigned char kSchemaVersionTypeByte = 0;
static const unsigned char kMaxDatabaseIdTypeByte = 1;
static const unsigned char kDataVersionTypeByte = 2;
static const unsigned char kMaxSimpleGlobalMetaDataTypeByte =
    3;  // Insert before this and increment.
static const unsigned char kDatabaseFreeListTypeByte = 100;
static const unsigned char kDatabaseNameTypeByte = 201;

static const unsigned char kObjectStoreMetaDataTypeByte = 50;
static const unsigned char kIndexMetaDataTypeByte = 100;
static const unsigned char kObjectStoreFreeListTypeByte = 150;
static const unsigned char kIndexFreeListTypeByte = 151;
static const unsigned char kObjectStoreNamesTypeByte = 200;
static const unsigned char kIndexNamesKeyTypeByte = 201;

static const unsigned char kObjectMetaDataTypeMaximum = 255;
static const unsigned char kIndexMetaDataTypeMaximum = 255;

const unsigned char kMinimumIndexId = 30;

inline void EncodeIntSafely(int64 nParam, int64 max, std::string* into) {
  DCHECK_LE(nParam, max);
  return EncodeInt(nParam, into);
}

std::string MaxIDBKey() {
  std::string ret;
  EncodeByte(kIndexedDBKeyNullTypeByte, &ret);
  return ret;
}

std::string MinIDBKey() {
  std::string ret;
  EncodeByte(kIndexedDBKeyMinKeyTypeByte, &ret);
  return ret;
}

void EncodeByte(unsigned char value, std::string* into) {
  into->push_back(value);
}

void EncodeBool(bool value, std::string* into) {
  into->push_back(value ? 1 : 0);
}

void EncodeInt(int64 value, std::string* into) {
#ifndef NDEBUG
  // Exercised by unit tests in debug only.
  DCHECK_GE(value, 0);
#endif
  uint64 n = static_cast<uint64>(value);

  do {
    unsigned char c = n;
    into->push_back(c);
    n >>= 8;
  } while (n);
}

void EncodeVarInt(int64 value, std::string* into) {
#ifndef NDEBUG
  // Exercised by unit tests in debug only.
  DCHECK_GE(value, 0);
#endif
  uint64 n = static_cast<uint64>(value);

  do {
    unsigned char c = n & 0x7f;
    n >>= 7;
    if (n)
      c |= 0x80;
    into->push_back(c);
  } while (n);
}

void EncodeString(const string16& value, std::string* into) {
  if (value.empty())
    return;
  // Backing store is UTF-16BE, convert from host endianness.
  size_t length = value.length();
  size_t current = into->size();
  into->resize(into->size() + length * sizeof(char16));

  const char16* src = value.c_str();
  char16* dst = reinterpret_cast<char16*>(&*into->begin() + current);
  for (unsigned i = 0; i < length; ++i)
    *dst++ = htons(*src++);
}

void EncodeStringWithLength(const string16& value, std::string* into) {
  EncodeVarInt(value.length(), into);
  EncodeString(value, into);
}

void EncodeDouble(double value, std::string* into) {
  // This always has host endianness.
  const char* p = reinterpret_cast<char*>(&value);
  into->insert(into->end(), p, p + sizeof(value));
}

void EncodeIDBKey(const IndexedDBKey& value, std::string* into) {
  size_t previous_size = into->size();
  DCHECK(value.IsValid());
  switch (value.type()) {
    case WebIDBKeyTypeNull:
    case WebIDBKeyTypeInvalid:
    case WebIDBKeyTypeMin: {
      NOTREACHED();
      EncodeByte(kIndexedDBKeyNullTypeByte, into);
      return;
    }
    case WebIDBKeyTypeArray: {
      EncodeByte(kIndexedDBKeyArrayTypeByte, into);
      size_t length = value.array().size();
      EncodeVarInt(length, into);
      for (size_t i = 0; i < length; ++i)
        EncodeIDBKey(value.array()[i], into);
      DCHECK_GT(into->size(), previous_size);
      return;
    }
    case WebIDBKeyTypeString: {
      EncodeByte(kIndexedDBKeyStringTypeByte, into);
      EncodeStringWithLength(value.string(), into);
      DCHECK_GT(into->size(), previous_size);
      return;
    }
    case WebIDBKeyTypeDate: {
      EncodeByte(kIndexedDBKeyDateTypeByte, into);
      EncodeDouble(value.date(), into);
      DCHECK_EQ(static_cast<size_t>(9),
                static_cast<size_t>(into->size() - previous_size));
      return;
    }
    case WebIDBKeyTypeNumber: {
      EncodeByte(kIndexedDBKeyNumberTypeByte, into);
      EncodeDouble(value.number(), into);
      DCHECK_EQ(static_cast<size_t>(9),
                static_cast<size_t>(into->size() - previous_size));
      return;
    }
  }

  NOTREACHED();
}

void EncodeIDBKeyPath(const IndexedDBKeyPath& value, std::string* into) {
  // May be typed, or may be a raw string. An invalid leading
  // byte is used to identify typed coding. New records are
  // always written as typed.
  EncodeByte(kIndexedDBKeyPathTypeCodedByte1, into);
  EncodeByte(kIndexedDBKeyPathTypeCodedByte2, into);
  EncodeByte(static_cast<char>(value.type()), into);
  switch (value.type()) {
    case WebIDBKeyPathTypeNull:
      break;
    case WebIDBKeyPathTypeString: {
      EncodeStringWithLength(value.string(), into);
      break;
    }
    case WebIDBKeyPathTypeArray: {
      const std::vector<string16>& array = value.array();
      size_t count = array.size();
      EncodeVarInt(count, into);
      for (size_t i = 0; i < count; ++i) {
        EncodeStringWithLength(array[i], into);
      }
      break;
    }
  }
}

bool DecodeByte(StringPiece* slice, unsigned char* value) {
  if (slice->empty())
    return false;

  *value = (*slice)[0];
  slice->remove_prefix(1);
  return true;
}

bool DecodeBool(StringPiece* slice, bool* value) {
  if (slice->empty())
    return false;

  *value = !!(*slice)[0];
  slice->remove_prefix(1);
  return true;
}

bool DecodeInt(StringPiece* slice, int64* value) {
  if (slice->empty())
    return false;

  StringPiece::const_iterator it = slice->begin();
  int shift = 0;
  int64 ret = 0;
  while (it != slice->end()) {
    unsigned char c = *it++;
    ret |= static_cast<int64>(c) << shift;
    shift += 8;
  }
  *value = ret;
  slice->remove_prefix(it - slice->begin());
  return true;
}

bool DecodeVarInt(StringPiece* slice, int64* value) {
  if (slice->empty())
    return false;

  StringPiece::const_iterator it = slice->begin();
  int shift = 0;
  int64 ret = 0;
  do {
    if (it == slice->end())
      return false;

    unsigned char c = *it;
    ret |= static_cast<int64>(c & 0x7f) << shift;
    shift += 7;
  } while (*it++ & 0x80);
  *value = ret;
  slice->remove_prefix(it - slice->begin());
  return true;
}

bool DecodeString(StringPiece* slice, string16* value) {
  if (slice->empty()) {
    value->clear();
    return true;
  }

  // Backing store is UTF-16BE, convert to host endianness.
  DCHECK(!(slice->size() % sizeof(char16)));
  size_t length = slice->size() / sizeof(char16);
  string16 decoded;
  decoded.reserve(length);
  const char16* encoded = reinterpret_cast<const char16*>(slice->begin());
  for (unsigned i = 0; i < length; ++i)
    decoded.push_back(ntohs(*encoded++));

  *value = decoded;
  slice->remove_prefix(length * sizeof(char16));
  return true;
}

bool DecodeStringWithLength(StringPiece* slice, string16* value) {
  if (slice->empty())
    return false;

  int64 length = 0;
  if (!DecodeVarInt(slice, &length) || length < 0)
    return false;
  size_t bytes = length * sizeof(char16);
  if (slice->size() < bytes)
    return false;

  StringPiece subpiece(slice->begin(), bytes);
  slice->remove_prefix(bytes);
  if (!DecodeString(&subpiece, value))
    return false;

  return true;
}

bool DecodeIDBKey(StringPiece* slice, scoped_ptr<IndexedDBKey>* value) {
  if (slice->empty())
    return false;

  unsigned char type = (*slice)[0];
  slice->remove_prefix(1);

  switch (type) {
    case kIndexedDBKeyNullTypeByte:
      *value = make_scoped_ptr(new IndexedDBKey());
      return true;

    case kIndexedDBKeyArrayTypeByte: {
      int64 length = 0;
      if (!DecodeVarInt(slice, &length) || length < 0)
        return false;
      IndexedDBKey::KeyArray array;
      while (length--) {
        scoped_ptr<IndexedDBKey> key;
        if (!DecodeIDBKey(slice, &key))
          return false;
        array.push_back(*key);
      }
      *value = make_scoped_ptr(new IndexedDBKey(array));
      return true;
    }
    case kIndexedDBKeyStringTypeByte: {
      string16 s;
      if (!DecodeStringWithLength(slice, &s))
        return false;
      *value = make_scoped_ptr(new IndexedDBKey(s));
      return true;
    }
    case kIndexedDBKeyDateTypeByte: {
      double d;
      if (!DecodeDouble(slice, &d))
        return false;
      *value = make_scoped_ptr(new IndexedDBKey(d, WebIDBKeyTypeDate));
      return true;
    }
    case kIndexedDBKeyNumberTypeByte: {
      double d;
      if (!DecodeDouble(slice, &d))
        return false;
      *value = make_scoped_ptr(new IndexedDBKey(d, WebIDBKeyTypeNumber));
      return true;
    }
  }

  NOTREACHED();
  return false;
}

bool DecodeDouble(StringPiece* slice, double* value) {
  if (slice->size() < sizeof(*value))
    return false;

  memcpy(value, slice->begin(), sizeof(*value));
  slice->remove_prefix(sizeof(*value));
  return true;
}

bool DecodeIDBKeyPath(StringPiece* slice, IndexedDBKeyPath* value) {
  // May be typed, or may be a raw string. An invalid leading
  // byte sequence is used to identify typed coding. New records are
  // always written as typed.
  if (slice->size() < 3 || (*slice)[0] != kIndexedDBKeyPathTypeCodedByte1 ||
      (*slice)[1] != kIndexedDBKeyPathTypeCodedByte2) {
    string16 s;
    if (!DecodeString(slice, &s))
      return false;
    *value = IndexedDBKeyPath(s);
    return true;
  }

  slice->remove_prefix(2);
  DCHECK(!slice->empty());
  WebIDBKeyPathType type = static_cast<WebIDBKeyPathType>((*slice)[0]);
  slice->remove_prefix(1);

  switch (type) {
    case WebIDBKeyPathTypeNull:
      DCHECK(slice->empty());
      *value = IndexedDBKeyPath();
      return true;
    case WebIDBKeyPathTypeString: {
      string16 string;
      if (!DecodeStringWithLength(slice, &string))
        return false;
      DCHECK(slice->empty());
      *value = IndexedDBKeyPath(string);
      return true;
    }
    case WebIDBKeyPathTypeArray: {
      std::vector<string16> array;
      int64 count;
      if (!DecodeVarInt(slice, &count))
        return false;
      DCHECK_GE(count, 0);
      while (count--) {
        string16 string;
        if (!DecodeStringWithLength(slice, &string))
          return false;
        array.push_back(string);
      }
      DCHECK(slice->empty());
      *value = IndexedDBKeyPath(array);
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool ConsumeEncodedIDBKey(StringPiece* slice) {
  unsigned char type = (*slice)[0];
  slice->remove_prefix(1);

  switch (type) {
    case kIndexedDBKeyNullTypeByte:
    case kIndexedDBKeyMinKeyTypeByte:
      return true;
    case kIndexedDBKeyArrayTypeByte: {
      int64 length;
      if (!DecodeVarInt(slice, &length))
        return false;
      while (length--) {
        if (!ConsumeEncodedIDBKey(slice))
          return false;
      }
      return true;
    }
    case kIndexedDBKeyStringTypeByte: {
      int64 length = 0;
      if (!DecodeVarInt(slice, &length) || length < 0)
        return false;
      if (slice->size() < static_cast<size_t>(length) * sizeof(char16))
        return false;
      slice->remove_prefix(length * sizeof(char16));
      return true;
    }
    case kIndexedDBKeyDateTypeByte:
    case kIndexedDBKeyNumberTypeByte:
      if (slice->size() < sizeof(double))
        return false;
      slice->remove_prefix(sizeof(double));
      return true;
  }
  NOTREACHED();
  return false;
}

bool ExtractEncodedIDBKey(StringPiece* slice, std::string* result) {
  const char* start = slice->begin();
  if (!ConsumeEncodedIDBKey(slice))
    return false;

  if (result)
    result->assign(start, slice->begin());
  return true;
}

static WebIDBKeyType KeyTypeByteToKeyType(unsigned char type) {
  switch (type) {
    case kIndexedDBKeyNullTypeByte:
      return WebIDBKeyTypeInvalid;
    case kIndexedDBKeyArrayTypeByte:
      return WebIDBKeyTypeArray;
    case kIndexedDBKeyStringTypeByte:
      return WebIDBKeyTypeString;
    case kIndexedDBKeyDateTypeByte:
      return WebIDBKeyTypeDate;
    case kIndexedDBKeyNumberTypeByte:
      return WebIDBKeyTypeNumber;
    case kIndexedDBKeyMinKeyTypeByte:
      return WebIDBKeyTypeMin;
  }

  NOTREACHED();
  return WebIDBKeyTypeInvalid;
}

int CompareEncodedStringsWithLength(StringPiece* slice1,
                                    StringPiece* slice2,
                                    bool* ok) {
  int64 len1, len2;
  if (!DecodeVarInt(slice1, &len1) || !DecodeVarInt(slice2, &len2)) {
    *ok = false;
    return 0;
  }
  DCHECK_GE(len1, 0);
  DCHECK_GE(len2, 0);
  if (len1 < 0 || len2 < 0) {
    *ok = false;
    return 0;
  }
  DCHECK_GE(slice1->size(), len1 * sizeof(char16));
  DCHECK_GE(slice2->size(), len2 * sizeof(char16));
  if (slice1->size() < len1 * sizeof(char16) ||
      slice2->size() < len2 * sizeof(char16)) {
    *ok = false;
    return 0;
  }

  StringPiece string1(slice1->begin(), len1 * sizeof(char16));
  StringPiece string2(slice2->begin(), len2 * sizeof(char16));
  slice1->remove_prefix(len1 * sizeof(char16));
  slice2->remove_prefix(len2 * sizeof(char16));

  *ok = true;
  // Strings are UTF-16BE encoded, so a simple memcmp is sufficient.
  return string1.compare(string2);
}

static int CompareInts(int64 a, int64 b) {
#ifndef NDEBUG
  // Exercised by unit tests in debug only.
  DCHECK_GE(a, 0);
  DCHECK_GE(b, 0);
#endif
  int64 diff = a - b;
  if (diff < 0)
    return -1;
  if (diff > 0)
    return 1;
  return 0;
}

static int CompareTypes(WebIDBKeyType a, WebIDBKeyType b) { return b - a; }

int CompareEncodedIDBKeys(StringPiece* slice_a,
                          StringPiece* slice_b,
                          bool* ok) {
  *ok = true;
  unsigned char type_a = (*slice_a)[0];
  unsigned char type_b = (*slice_b)[0];
  slice_a->remove_prefix(1);
  slice_b->remove_prefix(1);

  if (int x = CompareTypes(KeyTypeByteToKeyType(type_a),
                           KeyTypeByteToKeyType(type_b)))
    return x;

  switch (type_a) {
    case kIndexedDBKeyNullTypeByte:
    case kIndexedDBKeyMinKeyTypeByte:
      // Null type or max type; no payload to compare.
      return 0;
    case kIndexedDBKeyArrayTypeByte: {
      int64 length_a, length_b;
      if (!DecodeVarInt(slice_a, &length_a) ||
          !DecodeVarInt(slice_b, &length_b)) {
        *ok = false;
        return 0;
      }
      for (int64 i = 0; i < length_a && i < length_b; ++i) {
        int result = CompareEncodedIDBKeys(slice_a, slice_b, ok);
        if (!*ok || result)
          return result;
      }
      return length_a - length_b;
    }
    case kIndexedDBKeyStringTypeByte:
      return CompareEncodedStringsWithLength(slice_a, slice_b, ok);
    case kIndexedDBKeyDateTypeByte:
    case kIndexedDBKeyNumberTypeByte: {
      double d, e;
      if (!DecodeDouble(slice_a, &d) || !DecodeDouble(slice_b, &e)) {
        *ok = false;
        return 0;
      }
      if (d < e)
        return -1;
      if (d > e)
        return 1;
      return 0;
    }
  }

  NOTREACHED();
  return 0;
}

int CompareEncodedIDBKeys(const std::string& key_a,
                          const std::string& key_b,
                          bool* ok) {
  DCHECK(!key_a.empty());
  DCHECK(!key_b.empty());

  StringPiece slice_a(key_a);
  StringPiece slice_b(key_b);
  return CompareEncodedIDBKeys(&slice_a, &slice_b, ok);
}

namespace {

template <typename KeyType>
int Compare(const StringPiece& a, const StringPiece& b, bool, bool* ok) {
  KeyType key_a;
  KeyType key_b;

  StringPiece slice_a(a);
  if (!KeyType::Decode(&slice_a, &key_a)) {
    *ok = false;
    return 0;
  }
  StringPiece slice_b(b);
  if (!KeyType::Decode(&slice_b, &key_b)) {
    *ok = false;
    return 0;
  }

  *ok = true;
  return key_a.Compare(key_b);
}

template <>
int Compare<ExistsEntryKey>(const StringPiece& a,
                            const StringPiece& b,
                            bool,
                            bool* ok) {
  KeyPrefix prefix_a;
  KeyPrefix prefix_b;
  StringPiece slice_a(a);
  StringPiece slice_b(b);
  bool ok_a = KeyPrefix::Decode(&slice_a, &prefix_a);
  bool ok_b = KeyPrefix::Decode(&slice_b, &prefix_b);
  DCHECK(ok_a);
  DCHECK(ok_b);
  DCHECK(prefix_a.database_id_);
  DCHECK(prefix_a.object_store_id_);
  DCHECK_EQ(prefix_a.index_id_, ExistsEntryKey::kSpecialIndexNumber);
  DCHECK(prefix_b.database_id_);
  DCHECK(prefix_b.object_store_id_);
  DCHECK_EQ(prefix_b.index_id_, ExistsEntryKey::kSpecialIndexNumber);
  DCHECK(!slice_a.empty());
  DCHECK(!slice_b.empty());
  // Prefixes are not compared - it is assumed this was already done.
  DCHECK(!prefix_a.Compare(prefix_b));

  return CompareEncodedIDBKeys(&slice_a, &slice_b, ok);
}

template <>
int Compare<ObjectStoreDataKey>(const StringPiece& a,
                                const StringPiece& b,
                                bool,
                                bool* ok) {
  KeyPrefix prefix_a;
  KeyPrefix prefix_b;
  StringPiece slice_a(a);
  StringPiece slice_b(b);
  bool ok_a = KeyPrefix::Decode(&slice_a, &prefix_a);
  bool ok_b = KeyPrefix::Decode(&slice_b, &prefix_b);
  DCHECK(ok_a);
  DCHECK(ok_b);
  DCHECK(prefix_a.database_id_);
  DCHECK(prefix_a.object_store_id_);
  DCHECK_EQ(prefix_a.index_id_, ObjectStoreDataKey::kSpecialIndexNumber);
  DCHECK(prefix_b.database_id_);
  DCHECK(prefix_b.object_store_id_);
  DCHECK_EQ(prefix_b.index_id_, ObjectStoreDataKey::kSpecialIndexNumber);
  DCHECK(!slice_a.empty());
  DCHECK(!slice_b.empty());
  // Prefixes are not compared - it is assumed this was already done.
  DCHECK(!prefix_a.Compare(prefix_b));

  return CompareEncodedIDBKeys(&slice_a, &slice_b, ok);
}

template <>
int Compare<IndexDataKey>(const StringPiece& a,
                          const StringPiece& b,
                          bool ignore_duplicates,
                          bool* ok) {
  KeyPrefix prefix_a;
  KeyPrefix prefix_b;
  StringPiece slice_a(a);
  StringPiece slice_b(b);
  bool ok_a = KeyPrefix::Decode(&slice_a, &prefix_a);
  bool ok_b = KeyPrefix::Decode(&slice_b, &prefix_b);
  DCHECK(ok_a);
  DCHECK(ok_b);
  DCHECK(prefix_a.database_id_);
  DCHECK(prefix_a.object_store_id_);
  DCHECK_GE(prefix_a.index_id_, kMinimumIndexId);
  DCHECK(prefix_b.database_id_);
  DCHECK(prefix_b.object_store_id_);
  DCHECK_GE(prefix_b.index_id_, kMinimumIndexId);
  DCHECK(!slice_a.empty());
  DCHECK(!slice_b.empty());
  // Prefixes are not compared - it is assumed this was already done.
  DCHECK(!prefix_a.Compare(prefix_b));

  // index key
  int result = CompareEncodedIDBKeys(&slice_a, &slice_b, ok);
  if (!*ok || result)
    return result;
  if (ignore_duplicates)
    return 0;

  // sequence number [optional]
  int64 sequence_number_a = -1;
  int64 sequence_number_b = -1;
  if (!slice_a.empty()) {
    if (!DecodeVarInt(&slice_a, &sequence_number_a))
      return 0;
  }
  if (!slice_b.empty()) {
    if (!DecodeVarInt(&slice_b, &sequence_number_b))
      return 0;
  }

  // primary key [optional]
  if (slice_a.empty() && slice_b.empty())
    return 0;
  if (slice_a.empty())
    return -1;
  if (slice_b.empty())
    return 1;

  result = CompareEncodedIDBKeys(&slice_a, &slice_b, ok);
  if (!*ok || result)
    return result;

  return CompareInts(sequence_number_a, sequence_number_b);
}

int Compare(const StringPiece& a,
            const StringPiece& b,
            bool index_keys,
            bool* ok) {
  StringPiece slice_a(a);
  StringPiece slice_b(b);
  KeyPrefix prefix_a;
  KeyPrefix prefix_b;
  bool ok_a = KeyPrefix::Decode(&slice_a, &prefix_a);
  bool ok_b = KeyPrefix::Decode(&slice_b, &prefix_b);
  DCHECK(ok_a);
  DCHECK(ok_b);
  if (!ok_a || !ok_b) {
    *ok = false;
    return 0;
  }

  *ok = true;
  if (int x = prefix_a.Compare(prefix_b))
    return x;

  switch (prefix_a.type()) {
    case KeyPrefix::GLOBAL_METADATA: {
      DCHECK(!slice_a.empty());
      DCHECK(!slice_b.empty());

      unsigned char type_byte_a;
      if (!DecodeByte(&slice_a, &type_byte_a)) {
        *ok = false;
        return 0;
      }

      unsigned char type_byte_b;
      if (!DecodeByte(&slice_b, &type_byte_b)) {
        *ok = false;
        return 0;
      }

      if (int x = type_byte_a - type_byte_b)
        return x;
      if (type_byte_a < kMaxSimpleGlobalMetaDataTypeByte)
        return 0;

      const bool ignore_duplicates = false;
      if (type_byte_a == kDatabaseFreeListTypeByte)
        return Compare<DatabaseFreeListKey>(a, b, ignore_duplicates, ok);
      if (type_byte_a == kDatabaseNameTypeByte)
        return Compare<DatabaseNameKey>(a, b, ignore_duplicates, ok);
      break;
    }

    case KeyPrefix::DATABASE_METADATA: {
      DCHECK(!slice_a.empty());
      DCHECK(!slice_b.empty());

      unsigned char type_byte_a;
      if (!DecodeByte(&slice_a, &type_byte_a)) {
        *ok = false;
        return 0;
      }

      unsigned char type_byte_b;
      if (!DecodeByte(&slice_b, &type_byte_b)) {
        *ok = false;
        return 0;
      }

      if (int x = type_byte_a - type_byte_b)
        return x;
      if (type_byte_a < DatabaseMetaDataKey::MAX_SIMPLE_METADATA_TYPE)
        return 0;

      const bool ignore_duplicates = false;
      if (type_byte_a == kObjectStoreMetaDataTypeByte)
        return Compare<ObjectStoreMetaDataKey>(a, b, ignore_duplicates, ok);
      if (type_byte_a == kIndexMetaDataTypeByte)
        return Compare<IndexMetaDataKey>(a, b, ignore_duplicates, ok);
      if (type_byte_a == kObjectStoreFreeListTypeByte)
        return Compare<ObjectStoreFreeListKey>(a, b, ignore_duplicates, ok);
      if (type_byte_a == kIndexFreeListTypeByte)
        return Compare<IndexFreeListKey>(a, b, ignore_duplicates, ok);
      if (type_byte_a == kObjectStoreNamesTypeByte)
        return Compare<ObjectStoreNamesKey>(a, b, ignore_duplicates, ok);
      if (type_byte_a == kIndexNamesKeyTypeByte)
        return Compare<IndexNamesKey>(a, b, ignore_duplicates, ok);
      break;
    }

    case KeyPrefix::OBJECT_STORE_DATA: {
      if (slice_a.empty() || slice_b.empty())
        return slice_a.size() - slice_b.size();
      // TODO(jsbell): This case of non-existing user keys should not have to be
      // handled this way.

      const bool ignore_duplicates = false;
      return Compare<ObjectStoreDataKey>(a, b, ignore_duplicates, ok);
    }

    case KeyPrefix::EXISTS_ENTRY: {
      if (slice_a.empty() || slice_b.empty())
        return slice_a.size() - slice_b.size();
      // TODO(jsbell): This case of non-existing user keys should not have to be
      // handled this way.

      const bool ignore_duplicates = false;
      return Compare<ExistsEntryKey>(a, b, ignore_duplicates, ok);
    }

    case KeyPrefix::INDEX_DATA: {
      if (slice_a.empty() || slice_b.empty())
        return slice_a.size() - slice_b.size();
      // TODO(jsbell): This case of non-existing user keys should not have to be
      // handled this way.

      bool ignore_duplicates = index_keys;
      return Compare<IndexDataKey>(a, b, ignore_duplicates, ok);
    }

    case KeyPrefix::INVALID_TYPE:
      break;
  }

  NOTREACHED();
  *ok = false;
  return 0;
}

}  // namespace

int Compare(const StringPiece& a, const StringPiece& b, bool index_keys) {
  bool ok;
  int result = Compare(a, b, index_keys, &ok);
  DCHECK(ok);
  if (!ok)
    return 0;
  return result;
}

KeyPrefix::KeyPrefix()
    : database_id_(INVALID_TYPE),
      object_store_id_(INVALID_TYPE),
      index_id_(INVALID_TYPE) {}

KeyPrefix::KeyPrefix(int64 database_id)
    : database_id_(database_id), object_store_id_(0), index_id_(0) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
}

KeyPrefix::KeyPrefix(int64 database_id, int64 object_store_id)
    : database_id_(database_id),
      object_store_id_(object_store_id),
      index_id_(0) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
}

KeyPrefix::KeyPrefix(int64 database_id, int64 object_store_id, int64 index_id)
    : database_id_(database_id),
      object_store_id_(object_store_id),
      index_id_(index_id) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
  DCHECK(KeyPrefix::IsValidIndexId(index_id));
}

KeyPrefix::KeyPrefix(enum Type type,
                     int64 database_id,
                     int64 object_store_id,
                     int64 index_id)
    : database_id_(database_id),
      object_store_id_(object_store_id),
      index_id_(index_id) {
  DCHECK_EQ(type, INVALID_TYPE);
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
}

KeyPrefix KeyPrefix::CreateWithSpecialIndex(int64 database_id,
                                            int64 object_store_id,
                                            int64 index_id) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
  DCHECK(index_id);
  return KeyPrefix(INVALID_TYPE, database_id, object_store_id, index_id);
}

bool KeyPrefix::IsValidDatabaseId(int64 database_id) {
  return (database_id > 0) && (database_id < KeyPrefix::kMaxDatabaseId);
}

bool KeyPrefix::IsValidObjectStoreId(int64 object_store_id) {
  return (object_store_id > 0) &&
         (object_store_id < KeyPrefix::kMaxObjectStoreId);
}

bool KeyPrefix::IsValidIndexId(int64 index_id) {
  return (index_id >= kMinimumIndexId) && (index_id < KeyPrefix::kMaxIndexId);
}

bool KeyPrefix::Decode(StringPiece* slice, KeyPrefix* result) {
  unsigned char first_byte;
  if (!DecodeByte(slice, &first_byte))
    return false;

  size_t database_id_bytes = ((first_byte >> 5) & 0x7) + 1;
  size_t object_store_id_bytes = ((first_byte >> 2) & 0x7) + 1;
  size_t index_id_bytes = (first_byte & 0x3) + 1;

  if (database_id_bytes + object_store_id_bytes + index_id_bytes >
      slice->size())
    return false;

  {
    StringPiece tmp(slice->begin(), database_id_bytes);
    if (!DecodeInt(&tmp, &result->database_id_))
      return false;
  }
  slice->remove_prefix(database_id_bytes);
  {
    StringPiece tmp(slice->begin(), object_store_id_bytes);
    if (!DecodeInt(&tmp, &result->object_store_id_))
      return false;
  }
  slice->remove_prefix(object_store_id_bytes);
  {
    StringPiece tmp(slice->begin(), index_id_bytes);
    if (!DecodeInt(&tmp, &result->index_id_))
      return false;
  }
  slice->remove_prefix(index_id_bytes);
  return true;
}

std::string KeyPrefix::EncodeEmpty() {
  const std::string result(4, 0);
  DCHECK(EncodeInternal(0, 0, 0) == std::string(4, 0));
  return result;
}

std::string KeyPrefix::Encode() const {
  DCHECK(database_id_ != kInvalidId);
  DCHECK(object_store_id_ != kInvalidId);
  DCHECK(index_id_ != kInvalidId);
  return EncodeInternal(database_id_, object_store_id_, index_id_);
}

std::string KeyPrefix::EncodeInternal(int64 database_id,
                                      int64 object_store_id,
                                      int64 index_id) {
  std::string database_id_string;
  std::string object_store_id_string;
  std::string index_id_string;

  EncodeIntSafely(database_id, kMaxDatabaseId, &database_id_string);
  EncodeIntSafely(object_store_id, kMaxObjectStoreId, &object_store_id_string);
  EncodeIntSafely(index_id, kMaxIndexId, &index_id_string);

  DCHECK(database_id_string.size() <= kMaxDatabaseIdSizeBytes);
  DCHECK(object_store_id_string.size() <= kMaxObjectStoreIdSizeBytes);
  DCHECK(index_id_string.size() <= kMaxIndexIdSizeBytes);

  unsigned char first_byte =
      (database_id_string.size() - 1) << (kMaxObjectStoreIdSizeBits +
                                          kMaxIndexIdSizeBits) |
      (object_store_id_string.size() - 1) << kMaxIndexIdSizeBits |
      (index_id_string.size() - 1);
  COMPILE_ASSERT(kMaxDatabaseIdSizeBits + kMaxObjectStoreIdSizeBits +
                         kMaxIndexIdSizeBits ==
                     sizeof(first_byte) * 8,
                 CANT_ENCODE_IDS);
  std::string ret;
  ret.reserve(kDefaultInlineBufferSize);
  ret.push_back(first_byte);
  ret.append(database_id_string);
  ret.append(object_store_id_string);
  ret.append(index_id_string);

  DCHECK_LE(ret.size(), kDefaultInlineBufferSize);
  return ret;
}

int KeyPrefix::Compare(const KeyPrefix& other) const {
  DCHECK(database_id_ != kInvalidId);
  DCHECK(object_store_id_ != kInvalidId);
  DCHECK(index_id_ != kInvalidId);

  if (database_id_ != other.database_id_)
    return CompareInts(database_id_, other.database_id_);
  if (object_store_id_ != other.object_store_id_)
    return CompareInts(object_store_id_, other.object_store_id_);
  if (index_id_ != other.index_id_)
    return CompareInts(index_id_, other.index_id_);
  return 0;
}

KeyPrefix::Type KeyPrefix::type() const {
  DCHECK(database_id_ != kInvalidId);
  DCHECK(object_store_id_ != kInvalidId);
  DCHECK(index_id_ != kInvalidId);

  if (!database_id_)
    return GLOBAL_METADATA;
  if (!object_store_id_)
    return DATABASE_METADATA;
  if (index_id_ == kObjectStoreDataIndexId)
    return OBJECT_STORE_DATA;
  if (index_id_ == kExistsEntryIndexId)
    return EXISTS_ENTRY;
  if (index_id_ >= kMinimumIndexId)
    return INDEX_DATA;

  NOTREACHED();
  return INVALID_TYPE;
}

std::string SchemaVersionKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kSchemaVersionTypeByte);
  return ret;
}

std::string MaxDatabaseIdKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kMaxDatabaseIdTypeByte);
  return ret;
}

std::string DataVersionKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kDataVersionTypeByte);
  return ret;
}

DatabaseFreeListKey::DatabaseFreeListKey() : database_id_(-1) {}

bool DatabaseFreeListKey::Decode(StringPiece* slice,
                                 DatabaseFreeListKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(!prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kDatabaseFreeListTypeByte);
  if (!DecodeVarInt(slice, &result->database_id_))
    return false;
  return true;
}

std::string DatabaseFreeListKey::Encode(int64 database_id) {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kDatabaseFreeListTypeByte);
  EncodeVarInt(database_id, &ret);
  return ret;
}

std::string DatabaseFreeListKey::EncodeMaxKey() {
  return Encode(std::numeric_limits<int64>::max());
}

int64 DatabaseFreeListKey::DatabaseId() const {
  DCHECK_GE(database_id_, 0);
  return database_id_;
}

int DatabaseFreeListKey::Compare(const DatabaseFreeListKey& other) const {
  DCHECK_GE(database_id_, 0);
  return CompareInts(database_id_, other.database_id_);
}

bool DatabaseNameKey::Decode(StringPiece* slice, DatabaseNameKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(!prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kDatabaseNameTypeByte);
  if (!DecodeStringWithLength(slice, &result->origin_))
    return false;
  if (!DecodeStringWithLength(slice, &result->database_name_))
    return false;
  return true;
}

std::string DatabaseNameKey::Encode(const std::string& origin_identifier,
                                    const string16& database_name) {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kDatabaseNameTypeByte);
  EncodeStringWithLength(base::ASCIIToUTF16(origin_identifier), &ret);
  EncodeStringWithLength(database_name, &ret);
  return ret;
}

std::string DatabaseNameKey::EncodeMinKeyForOrigin(
    const std::string& origin_identifier) {
  return Encode(origin_identifier, string16());
}

std::string DatabaseNameKey::EncodeStopKeyForOrigin(
    const std::string& origin_identifier) {
  // just after origin in collation order
  return EncodeMinKeyForOrigin(origin_identifier + '\x01');
}

int DatabaseNameKey::Compare(const DatabaseNameKey& other) {
  if (int x = origin_.compare(other.origin_))
    return x;
  return database_name_.compare(other.database_name_);
}

std::string DatabaseMetaDataKey::Encode(int64 database_id,
                                        MetaDataType meta_data_type) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(meta_data_type);
  return ret;
}

ObjectStoreMetaDataKey::ObjectStoreMetaDataKey()
    : object_store_id_(-1), meta_data_type_(-1) {}

bool ObjectStoreMetaDataKey::Decode(StringPiece* slice,
                                    ObjectStoreMetaDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kObjectStoreMetaDataTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  DCHECK(result->object_store_id_);
  if (!DecodeByte(slice, &result->meta_data_type_))
    return false;
  return true;
}

std::string ObjectStoreMetaDataKey::Encode(int64 database_id,
                                           int64 object_store_id,
                                           unsigned char meta_data_type) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kObjectStoreMetaDataTypeByte);
  EncodeVarInt(object_store_id, &ret);
  ret.push_back(meta_data_type);
  return ret;
}

std::string ObjectStoreMetaDataKey::EncodeMaxKey(int64 database_id) {
  return Encode(database_id,
                std::numeric_limits<int64>::max(),
                kObjectMetaDataTypeMaximum);
}

std::string ObjectStoreMetaDataKey::EncodeMaxKey(int64 database_id,
                                                 int64 object_store_id) {
  return Encode(database_id, object_store_id, kObjectMetaDataTypeMaximum);
}

int64 ObjectStoreMetaDataKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}
unsigned char ObjectStoreMetaDataKey::MetaDataType() const {
  return meta_data_type_;
}

int ObjectStoreMetaDataKey::Compare(const ObjectStoreMetaDataKey& other) {
  DCHECK_GE(object_store_id_, 0);
  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  int64 result = meta_data_type_ - other.meta_data_type_;
  if (result < 0)
    return -1;
  return (result > 0) ? 1 : result;
}

IndexMetaDataKey::IndexMetaDataKey()
    : object_store_id_(-1), index_id_(-1), meta_data_type_(0) {}

bool IndexMetaDataKey::Decode(StringPiece* slice, IndexMetaDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kIndexMetaDataTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  if (!DecodeVarInt(slice, &result->index_id_))
    return false;
  if (!DecodeByte(slice, &result->meta_data_type_))
    return false;
  return true;
}

std::string IndexMetaDataKey::Encode(int64 database_id,
                                     int64 object_store_id,
                                     int64 index_id,
                                     unsigned char meta_data_type) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kIndexMetaDataTypeByte);
  EncodeVarInt(object_store_id, &ret);
  EncodeVarInt(index_id, &ret);
  EncodeByte(meta_data_type, &ret);
  return ret;
}

std::string IndexMetaDataKey::EncodeMaxKey(int64 database_id,
                                           int64 object_store_id) {
  return Encode(database_id,
                object_store_id,
                std::numeric_limits<int64>::max(),
                kIndexMetaDataTypeMaximum);
}

std::string IndexMetaDataKey::EncodeMaxKey(int64 database_id,
                                           int64 object_store_id,
                                           int64 index_id) {
  return Encode(
      database_id, object_store_id, index_id, kIndexMetaDataTypeMaximum);
}

int IndexMetaDataKey::Compare(const IndexMetaDataKey& other) {
  DCHECK_GE(object_store_id_, 0);
  DCHECK_GE(index_id_, 0);

  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  if (int x = CompareInts(index_id_, other.index_id_))
    return x;
  return meta_data_type_ - other.meta_data_type_;
}

int64 IndexMetaDataKey::IndexId() const {
  DCHECK_GE(index_id_, 0);
  return index_id_;
}

ObjectStoreFreeListKey::ObjectStoreFreeListKey() : object_store_id_(-1) {}

bool ObjectStoreFreeListKey::Decode(StringPiece* slice,
                                    ObjectStoreFreeListKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kObjectStoreFreeListTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  return true;
}

std::string ObjectStoreFreeListKey::Encode(int64 database_id,
                                           int64 object_store_id) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kObjectStoreFreeListTypeByte);
  EncodeVarInt(object_store_id, &ret);
  return ret;
}

std::string ObjectStoreFreeListKey::EncodeMaxKey(int64 database_id) {
  return Encode(database_id, std::numeric_limits<int64>::max());
}

int64 ObjectStoreFreeListKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}

int ObjectStoreFreeListKey::Compare(const ObjectStoreFreeListKey& other) {
  // TODO(jsbell): It may seem strange that we're not comparing database id's,
  // but that comparison will have been made earlier.
  // We should probably make this more clear, though...
  DCHECK_GE(object_store_id_, 0);
  return CompareInts(object_store_id_, other.object_store_id_);
}

IndexFreeListKey::IndexFreeListKey() : object_store_id_(-1), index_id_(-1) {}

bool IndexFreeListKey::Decode(StringPiece* slice, IndexFreeListKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kIndexFreeListTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  if (!DecodeVarInt(slice, &result->index_id_))
    return false;
  return true;
}

std::string IndexFreeListKey::Encode(int64 database_id,
                                     int64 object_store_id,
                                     int64 index_id) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kIndexFreeListTypeByte);
  EncodeVarInt(object_store_id, &ret);
  EncodeVarInt(index_id, &ret);
  return ret;
}

std::string IndexFreeListKey::EncodeMaxKey(int64 database_id,
                                           int64 object_store_id) {
  return Encode(
      database_id, object_store_id, std::numeric_limits<int64>::max());
}

int IndexFreeListKey::Compare(const IndexFreeListKey& other) {
  DCHECK_GE(object_store_id_, 0);
  DCHECK_GE(index_id_, 0);
  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  return CompareInts(index_id_, other.index_id_);
}

int64 IndexFreeListKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}

int64 IndexFreeListKey::IndexId() const {
  DCHECK_GE(index_id_, 0);
  return index_id_;
}

// TODO(jsbell): We never use this to look up object store ids,
// because a mapping is kept in the IndexedDBDatabase. Can the
// mapping become unreliable?  Can we remove this?
bool ObjectStoreNamesKey::Decode(StringPiece* slice,
                                 ObjectStoreNamesKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kObjectStoreNamesTypeByte);
  if (!DecodeStringWithLength(slice, &result->object_store_name_))
    return false;
  return true;
}

std::string ObjectStoreNamesKey::Encode(int64 database_id,
                                        const string16& object_store_name) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kObjectStoreNamesTypeByte);
  EncodeStringWithLength(object_store_name, &ret);
  return ret;
}

int ObjectStoreNamesKey::Compare(const ObjectStoreNamesKey& other) {
  return object_store_name_.compare(other.object_store_name_);
}

IndexNamesKey::IndexNamesKey() : object_store_id_(-1) {}

// TODO(jsbell): We never use this to look up index ids, because a mapping
// is kept at a higher level.
bool IndexNamesKey::Decode(StringPiece* slice, IndexNamesKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kIndexNamesKeyTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  if (!DecodeStringWithLength(slice, &result->index_name_))
    return false;
  return true;
}

std::string IndexNamesKey::Encode(int64 database_id,
                                  int64 object_store_id,
                                  const string16& index_name) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kIndexNamesKeyTypeByte);
  EncodeVarInt(object_store_id, &ret);
  EncodeStringWithLength(index_name, &ret);
  return ret;
}

int IndexNamesKey::Compare(const IndexNamesKey& other) {
  DCHECK_GE(object_store_id_, 0);
  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  return index_name_.compare(other.index_name_);
}

ObjectStoreDataKey::ObjectStoreDataKey() {}
ObjectStoreDataKey::~ObjectStoreDataKey() {}

bool ObjectStoreDataKey::Decode(StringPiece* slice,
                                ObjectStoreDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_EQ(prefix.index_id_, kSpecialIndexNumber);
  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;
  return true;
}

std::string ObjectStoreDataKey::Encode(int64 database_id,
                                       int64 object_store_id,
                                       const std::string encoded_user_key) {
  KeyPrefix prefix(KeyPrefix::CreateWithSpecialIndex(
      database_id, object_store_id, kSpecialIndexNumber));
  std::string ret = prefix.Encode();
  ret.append(encoded_user_key);

  return ret;
}

std::string ObjectStoreDataKey::Encode(int64 database_id,
                                       int64 object_store_id,
                                       const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(database_id, object_store_id, encoded_key);
}

int ObjectStoreDataKey::Compare(const ObjectStoreDataKey& other, bool* ok) {
  return CompareEncodedIDBKeys(encoded_user_key_, other.encoded_user_key_, ok);
}

scoped_ptr<IndexedDBKey> ObjectStoreDataKey::user_key() const {
  scoped_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key.Pass();
}

const int64 ObjectStoreDataKey::kSpecialIndexNumber = kObjectStoreDataIndexId;

ExistsEntryKey::ExistsEntryKey() {}
ExistsEntryKey::~ExistsEntryKey() {}

bool ExistsEntryKey::Decode(StringPiece* slice, ExistsEntryKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_EQ(prefix.index_id_, kSpecialIndexNumber);
  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;
  return true;
}

std::string ExistsEntryKey::Encode(int64 database_id,
                                   int64 object_store_id,
                                   const std::string& encoded_key) {
  KeyPrefix prefix(KeyPrefix::CreateWithSpecialIndex(
      database_id, object_store_id, kSpecialIndexNumber));
  std::string ret = prefix.Encode();
  ret.append(encoded_key);
  return ret;
}

std::string ExistsEntryKey::Encode(int64 database_id,
                                   int64 object_store_id,
                                   const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(database_id, object_store_id, encoded_key);
}

int ExistsEntryKey::Compare(const ExistsEntryKey& other, bool* ok) {
  return CompareEncodedIDBKeys(encoded_user_key_, other.encoded_user_key_, ok);
}

scoped_ptr<IndexedDBKey> ExistsEntryKey::user_key() const {
  scoped_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key.Pass();
}

const int64 ExistsEntryKey::kSpecialIndexNumber = kExistsEntryIndexId;

IndexDataKey::IndexDataKey()
    : database_id_(-1),
      object_store_id_(-1),
      index_id_(-1),
      sequence_number_(-1) {}

IndexDataKey::~IndexDataKey() {}

bool IndexDataKey::Decode(StringPiece* slice, IndexDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_GE(prefix.index_id_, kMinimumIndexId);
  result->database_id_ = prefix.database_id_;
  result->object_store_id_ = prefix.object_store_id_;
  result->index_id_ = prefix.index_id_;
  result->sequence_number_ = -1;
  result->encoded_primary_key_ = MinIDBKey();

  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;

  // [optional] sequence number
  if (slice->empty())
    return true;
  if (!DecodeVarInt(slice, &result->sequence_number_))
    return false;

  // [optional] primary key
  if (slice->empty())
    return true;
  if (!ExtractEncodedIDBKey(slice, &result->encoded_primary_key_))
    return false;
  return true;
}

std::string IndexDataKey::Encode(int64 database_id,
                                 int64 object_store_id,
                                 int64 index_id,
                                 const std::string& encoded_user_key,
                                 const std::string& encoded_primary_key,
                                 int64 sequence_number) {
  KeyPrefix prefix(database_id, object_store_id, index_id);
  std::string ret = prefix.Encode();
  ret.append(encoded_user_key);
  EncodeVarInt(sequence_number, &ret);
  ret.append(encoded_primary_key);
  return ret;
}

std::string IndexDataKey::Encode(int64 database_id,
                                 int64 object_store_id,
                                 int64 index_id,
                                 const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(
      database_id, object_store_id, index_id, encoded_key, MinIDBKey(), 0);
}

std::string IndexDataKey::EncodeMinKey(int64 database_id,
                                       int64 object_store_id,
                                       int64 index_id) {
  return Encode(
      database_id, object_store_id, index_id, MinIDBKey(), MinIDBKey(), 0);
}

std::string IndexDataKey::EncodeMaxKey(int64 database_id,
                                       int64 object_store_id,
                                       int64 index_id) {
  return Encode(database_id,
                object_store_id,
                index_id,
                MaxIDBKey(),
                MaxIDBKey(),
                std::numeric_limits<int64>::max());
}

int IndexDataKey::Compare(const IndexDataKey& other,
                          bool ignore_duplicates,
                          bool* ok) {
  DCHECK_GE(database_id_, 0);
  DCHECK_GE(object_store_id_, 0);
  DCHECK_GE(index_id_, 0);
  int result =
      CompareEncodedIDBKeys(encoded_user_key_, other.encoded_user_key_, ok);
  if (!*ok || result)
    return result;
  if (ignore_duplicates)
    return 0;
  result = CompareEncodedIDBKeys(
      encoded_primary_key_, other.encoded_primary_key_, ok);
  if (!*ok || result)
    return result;
  return CompareInts(sequence_number_, other.sequence_number_);
}

int64 IndexDataKey::DatabaseId() const {
  DCHECK_GE(database_id_, 0);
  return database_id_;
}

int64 IndexDataKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}

int64 IndexDataKey::IndexId() const {
  DCHECK_GE(index_id_, 0);
  return index_id_;
}

scoped_ptr<IndexedDBKey> IndexDataKey::user_key() const {
  scoped_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key.Pass();
}

scoped_ptr<IndexedDBKey> IndexDataKey::primary_key() const {
  scoped_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_primary_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key.Pass();
}

}  // namespace content
