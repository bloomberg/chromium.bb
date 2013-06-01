// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"

namespace content {

class LevelDBSlice;

CONTENT_EXPORT extern const unsigned char kMinimumIndexId;

CONTENT_EXPORT std::vector<char> EncodeByte(unsigned char);
CONTENT_EXPORT const char* DecodeByte(const char* p,
                                      const char* limit,
                                      unsigned char& found_char);
CONTENT_EXPORT std::vector<char> MaxIDBKey();
CONTENT_EXPORT std::vector<char> MinIDBKey();
CONTENT_EXPORT std::vector<char> EncodeBool(bool value);
CONTENT_EXPORT bool DecodeBool(const char* begin, const char* end);
CONTENT_EXPORT std::vector<char> EncodeInt(int64);
inline std::vector<char> EncodeIntSafely(int64 nParam, int64 max) {
  DCHECK_LE(nParam, max);
  return EncodeInt(nParam);
}

template <class T>
int64 DecodeInt(T begin, T end) {
  // TODO(alecflett): Make this a DCHECK_LE().
  DCHECK(begin <= end);
  int64 ret = 0;

  int shift = 0;
  while (begin < end) {
    unsigned char c = *begin++;
    ret |= static_cast<int64>(c) << shift;
    shift += 8;
  }

  return ret;
}

CONTENT_EXPORT std::vector<char> EncodeVarInt(int64);
CONTENT_EXPORT const char* DecodeVarInt(const char* p,
                                        const char* limit,
                                        int64& found_int);
CONTENT_EXPORT std::vector<char> EncodeString(const string16& string);
CONTENT_EXPORT string16 DecodeString(const char* p, const char* end);
CONTENT_EXPORT std::vector<char> EncodeStringWithLength(const string16& string);
CONTENT_EXPORT const char* DecodeStringWithLength(const char* p,
                                                  const char* limit,
                                                  string16& found_string);
CONTENT_EXPORT int CompareEncodedStringsWithLength(const char*& p,
                                                   const char* limit_p,
                                                   const char*& q,
                                                   const char* limit_q,
                                                   bool& ok);
CONTENT_EXPORT std::vector<char> EncodeDouble(double value);
CONTENT_EXPORT const char* DecodeDouble(const char* p,
                                        const char* limit,
                                        double* value);
void EncodeIDBKey(const IndexedDBKey& key, std::vector<char>& into);
CONTENT_EXPORT std::vector<char> EncodeIDBKey(const IndexedDBKey& key);
CONTENT_EXPORT const char* DecodeIDBKey(const char* p,
                                        const char* limit,
                                        scoped_ptr<IndexedDBKey>* found_key);
CONTENT_EXPORT const char* ExtractEncodedIDBKey(const char* start,
                                                const char* limit,
                                                std::vector<char>* result);
CONTENT_EXPORT int CompareEncodedIDBKeys(const std::vector<char>& a,
                                         const std::vector<char>& b,
                                         bool& ok);
CONTENT_EXPORT std::vector<char> EncodeIDBKeyPath(
    const IndexedDBKeyPath& key_path);
CONTENT_EXPORT IndexedDBKeyPath DecodeIDBKeyPath(const char* start,
                                                 const char* limit);

CONTENT_EXPORT int Compare(const LevelDBSlice& a,
                           const LevelDBSlice& b,
                           bool index_keys = false);

class KeyPrefix {
 public:
  KeyPrefix();
  explicit KeyPrefix(int64 database_id);
  KeyPrefix(int64 database_id, int64 object_store_id);
  KeyPrefix(int64 database_id, int64 object_store_id, int64 index_id);
  static KeyPrefix CreateWithSpecialIndex(int64 database_id,
                                          int64 object_store_id,
                                          int64 index_id);

  static const char* Decode(const char* start,
                            const char* limit,
                            KeyPrefix* result);
  std::vector<char> Encode() const;
  static std::vector<char> EncodeEmpty();
  int Compare(const KeyPrefix& other) const;

  enum Type {
    GLOBAL_METADATA,
    DATABASE_METADATA,
    OBJECT_STORE_DATA,
    EXISTS_ENTRY,
    INDEX_DATA,
    INVALID_TYPE
  };

  static const size_t kMaxDatabaseIdSizeBits = 3;
  static const size_t kMaxObjectStoreIdSizeBits = 3;
  static const size_t kMaxIndexIdSizeBits = 2;

  static const size_t kMaxDatabaseIdSizeBytes =
      1ULL << kMaxDatabaseIdSizeBits;  // 8
  static const size_t kMaxObjectStoreIdSizeBytes =
      1ULL << kMaxObjectStoreIdSizeBits;                                   // 8
  static const size_t kMaxIndexIdSizeBytes = 1ULL << kMaxIndexIdSizeBits;  // 4

  static const size_t kMaxDatabaseIdBits =
      kMaxDatabaseIdSizeBytes * 8 - 1;  // 63
  static const size_t kMaxObjectStoreIdBits =
      kMaxObjectStoreIdSizeBytes * 8 - 1;                              // 63
  static const size_t kMaxIndexIdBits = kMaxIndexIdSizeBytes * 8 - 1;  // 31

  static const int64 kMaxDatabaseId =
      (1ULL << kMaxDatabaseIdBits) - 1;  // max signed int64
  static const int64 kMaxObjectStoreId =
      (1ULL << kMaxObjectStoreIdBits) - 1;  // max signed int64
  static const int64 kMaxIndexId =
      (1ULL << kMaxIndexIdBits) - 1;  // max signed int32

  static bool IsValidDatabaseId(int64 database_id);
  static bool IsValidObjectStoreId(int64 index_id);
  static bool IsValidIndexId(int64 index_id);
  static bool ValidIds(int64 database_id,
                       int64 object_store_id,
                       int64 index_id) {
    return IsValidDatabaseId(database_id) &&
           IsValidObjectStoreId(object_store_id) && IsValidIndexId(index_id);
  }
  static bool ValidIds(int64 database_id, int64 object_store_id) {
    return IsValidDatabaseId(database_id) &&
           IsValidObjectStoreId(object_store_id);
  }

  Type type() const;

  int64 database_id_;
  int64 object_store_id_;
  int64 index_id_;

  static const int64 kInvalidId = -1;

 private:
  static std::vector<char> EncodeInternal(int64 database_id,
                                          int64 object_store_id,
                                          int64 index_id);
  // Special constructor for CreateWithSpecialIndex()
  KeyPrefix(enum Type,
            int64 database_id,
            int64 object_store_id,
            int64 index_id);
};

class SchemaVersionKey {
 public:
  CONTENT_EXPORT static std::vector<char> Encode();
};

class MaxDatabaseIdKey {
 public:
  CONTENT_EXPORT static std::vector<char> Encode();
};

class DataVersionKey {
 public:
  static std::vector<char> Encode();
};

class DatabaseFreeListKey {
 public:
  DatabaseFreeListKey();
  static const char* Decode(const char* start,
                            const char* limit,
                            DatabaseFreeListKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id);
  static CONTENT_EXPORT std::vector<char> EncodeMaxKey();
  int64 DatabaseId() const;
  int Compare(const DatabaseFreeListKey& other) const;

 private:
  int64 database_id_;
};

class DatabaseNameKey {
 public:
  static const char* Decode(const char* start,
                            const char* limit,
                            DatabaseNameKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(const string16& origin,
                                                 const string16& database_name);
  static std::vector<char> EncodeMinKeyForOrigin(const string16& origin);
  static std::vector<char> EncodeStopKeyForOrigin(const string16& origin);
  string16 origin() const { return origin_; }
  string16 database_name() const { return database_name_; }
  int Compare(const DatabaseNameKey& other);

 private:
  string16 origin_;  // TODO(jsbell): Store encoded strings, or just pointers.
  string16 database_name_;
};

class DatabaseMetaDataKey {
 public:
  enum MetaDataType {
    ORIGIN_NAME = 0,
    DATABASE_NAME = 1,
    USER_VERSION = 2,
    MAX_OBJECT_STORE_ID = 3,
    USER_INT_VERSION = 4,
    MAX_SIMPLE_METADATA_TYPE = 5
  };

  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id,
                                                 MetaDataType type);
};

class ObjectStoreMetaDataKey {
 public:
  enum MetaDataType {
    NAME = 0,
    KEY_PATH = 1,
    AUTO_INCREMENT = 2,
    EVICTABLE = 3,
    LAST_VERSION = 4,
    MAX_INDEX_ID = 5,
    HAS_KEY_PATH = 6,
    KEY_GENERATOR_CURRENT_NUMBER = 7
  };

  ObjectStoreMetaDataKey();
  static const char* Decode(const char* start,
                            const char* limit,
                            ObjectStoreMetaDataKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id,
                                                 int64 object_store_id,
                                                 unsigned char meta_data_type);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id,
                                                       int64 object_store_id);
  int64 ObjectStoreId() const;
  unsigned char MetaDataType() const;
  int Compare(const ObjectStoreMetaDataKey& other);

 private:
  int64 object_store_id_;
  unsigned char meta_data_type_;
};

class IndexMetaDataKey {
 public:
  enum MetaDataType {
    NAME = 0,
    UNIQUE = 1,
    KEY_PATH = 2,
    MULTI_ENTRY = 3
  };

  IndexMetaDataKey();
  static const char* Decode(const char* start,
                            const char* limit,
                            IndexMetaDataKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id,
                                                 int64 object_store_id,
                                                 int64 index_id,
                                                 unsigned char meta_data_type);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id,
                                                       int64 object_store_id);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id,
                                                       int64 object_store_id,
                                                       int64 index_id);
  int Compare(const IndexMetaDataKey& other);
  int64 IndexId() const;
  unsigned char meta_data_type() const { return meta_data_type_; }

 private:
  int64 object_store_id_;
  int64 index_id_;
  unsigned char meta_data_type_;
};

class ObjectStoreFreeListKey {
 public:
  ObjectStoreFreeListKey();
  static const char* Decode(const char* start,
                            const char* limit,
                            ObjectStoreFreeListKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id,
                                                 int64 object_store_id);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id);
  int64 ObjectStoreId() const;
  int Compare(const ObjectStoreFreeListKey& other);

 private:
  int64 object_store_id_;
};

class IndexFreeListKey {
 public:
  IndexFreeListKey();
  static const char* Decode(const char* start,
                            const char* limit,
                            IndexFreeListKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id,
                                                 int64 object_store_id,
                                                 int64 index_id);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id,
                                                       int64 object_store_id);
  int Compare(const IndexFreeListKey& other);
  int64 ObjectStoreId() const;
  int64 IndexId() const;

 private:
  int64 object_store_id_;
  int64 index_id_;
};

class ObjectStoreNamesKey {
 public:
  // TODO(jsbell): We never use this to look up object store ids,
  // because a mapping is kept in the IndexedDBDatabaseImpl. Can the
  // mapping become unreliable?  Can we remove this?
  static const char* Decode(const char* start,
                            const char* limit,
                            ObjectStoreNamesKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(
      int64 database_id,
      const string16& object_store_name);
  int Compare(const ObjectStoreNamesKey& other);
  string16 object_store_name() const { return object_store_name_; }

 private:
  string16
      object_store_name_;  // TODO(jsbell): Store the encoded string, or just
                           // pointers to it.
};

class IndexNamesKey {
 public:
  IndexNamesKey();
  // TODO(jsbell): We never use this to look up index ids, because a mapping
  // is kept at a higher level.
  static const char* Decode(const char* start,
                            const char* limit,
                            IndexNamesKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(int64 database_id,
                                                 int64 object_store_id,
                                                 const string16& index_name);
  int Compare(const IndexNamesKey& other);
  string16 index_name() const { return index_name_; }

 private:
  int64 object_store_id_;
  string16 index_name_;
};

class ObjectStoreDataKey {
 public:
  static const char* Decode(const char* start,
                            const char* end,
                            ObjectStoreDataKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(
      int64 database_id,
      int64 object_store_id,
      const std::vector<char> encoded_user_key);
  static std::vector<char> Encode(int64 database_id,
                                  int64 object_store_id,
                                  const IndexedDBKey& user_key);
  int Compare(const ObjectStoreDataKey& other, bool& ok);
  scoped_ptr<IndexedDBKey> user_key() const;
  static const int64 kSpecialIndexNumber;
  ObjectStoreDataKey();
  ~ObjectStoreDataKey();

 private:
  std::vector<char> encoded_user_key_;
};

class ExistsEntryKey {
 public:
  ExistsEntryKey();
  ~ExistsEntryKey();

  static const char* Decode(const char* start,
                            const char* end,
                            ExistsEntryKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(
      int64 database_id,
      int64 object_store_id,
      const std::vector<char>& encoded_key);
  static std::vector<char> Encode(int64 database_id,
                                  int64 object_store_id,
                                  const IndexedDBKey& user_key);
  int Compare(const ExistsEntryKey& other, bool& ok);
  scoped_ptr<IndexedDBKey> user_key() const;

  static const int64 kSpecialIndexNumber;

 private:
  std::vector<char> encoded_user_key_;
  DISALLOW_COPY_AND_ASSIGN(ExistsEntryKey);
};

class IndexDataKey {
 public:
  IndexDataKey();
  ~IndexDataKey();
  static const char* Decode(const char* start,
                            const char* limit,
                            IndexDataKey* result);
  CONTENT_EXPORT static std::vector<char> Encode(
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const std::vector<char>& encoded_user_key,
      const std::vector<char>& encoded_primary_key,
      int64 sequence_number = 0);
  static std::vector<char> Encode(int64 database_id,
                                  int64 object_store_id,
                                  int64 index_id,
                                  const IndexedDBKey& user_key);
  static std::vector<char> EncodeMinKey(int64 database_id,
                                        int64 object_store_id,
                                        int64 index_id);
  CONTENT_EXPORT static std::vector<char> EncodeMaxKey(int64 database_id,
                                                       int64 object_store_id,
                                                       int64 index_id);
  int Compare(const IndexDataKey& other, bool ignore_duplicates, bool& ok);
  int64 DatabaseId() const;
  int64 ObjectStoreId() const;
  int64 IndexId() const;
  scoped_ptr<IndexedDBKey> user_key() const;
  scoped_ptr<IndexedDBKey> primary_key() const;

 private:
  int64 database_id_;
  int64 object_store_id_;
  int64 index_id_;
  std::vector<char> encoded_user_key_;
  std::vector<char> encoded_primary_key_;
  int64 sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(IndexDataKey);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_
