// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_backing_store.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValueVersion.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"
#include "webkit/browser/fileapi/local_file_stream_writer.h"
#include "webkit/common/database/database_identifier.h"

using base::FilePath;
using base::StringPiece;
using fileapi::FileWriterDelegate;

namespace content {

namespace {

FilePath GetBlobDirectoryName(const FilePath& pathBase, int64 database_id) {
  return pathBase.AppendASCII(base::StringPrintf("%" PRIx64, database_id));
}

FilePath GetBlobDirectoryNameForKey(const FilePath& pathBase,
                                    int64 database_id,
                                    int64 key) {
  FilePath path = GetBlobDirectoryName(pathBase, database_id);
  path = path.AppendASCII(base::StringPrintf(
      "%02x", static_cast<int>(key & 0x000000000000ff00) >> 8));
  return path;
}

FilePath GetBlobFileNameForKey(const FilePath& pathBase,
                               int64 database_id,
                               int64 key) {
  FilePath path = GetBlobDirectoryNameForKey(pathBase, database_id, key);
  path = path.AppendASCII(base::StringPrintf("%" PRIx64, key));
  return path;
}

bool MakeIDBBlobDirectory(const FilePath& pathBase,
                          int64 database_id,
                          int64 key) {
  FilePath path = GetBlobDirectoryNameForKey(pathBase, database_id, key);
  return base::CreateDirectory(path);
}

static std::string ComputeOriginIdentifier(const GURL& origin_url) {
  return webkit_database::GetIdentifierFromOrigin(origin_url) + "@1";
}

static base::FilePath ComputeFileName(const GURL& origin_url) {
  return base::FilePath()
      .AppendASCII(webkit_database::GetIdentifierFromOrigin(origin_url))
      .AddExtension(FILE_PATH_LITERAL(".indexeddb.leveldb"));
}

static base::FilePath ComputeBlobPath(const GURL& origin_url) {
  return base::FilePath()
      .AppendASCII(webkit_database::GetIdentifierFromOrigin(origin_url))
      .AddExtension(FILE_PATH_LITERAL(".indexeddb.blob"));
}

static base::FilePath ComputeCorruptionFileName(const GURL& origin_url) {
  return ComputeFileName(origin_url)
      .Append(FILE_PATH_LITERAL("corruption_info.json"));
}

}  // namespace

static const int64 kKeyGeneratorInitialNumber =
    1;  // From the IndexedDB specification.

enum IndexedDBBackingStoreErrorSource {
  // 0 - 2 are no longer used.
  FIND_KEY_IN_INDEX = 3,
  GET_IDBDATABASE_METADATA,
  GET_INDEXES,
  GET_KEY_GENERATOR_CURRENT_NUMBER,
  GET_OBJECT_STORES,
  GET_RECORD,
  KEY_EXISTS_IN_OBJECT_STORE,
  LOAD_CURRENT_ROW,
  SET_UP_METADATA,
  GET_PRIMARY_KEY_VIA_INDEX,
  KEY_EXISTS_IN_INDEX,
  VERSION_EXISTS,
  DELETE_OBJECT_STORE,
  SET_MAX_OBJECT_STORE_ID,
  SET_MAX_INDEX_ID,
  GET_NEW_DATABASE_ID,
  GET_NEW_VERSION_NUMBER,
  CREATE_IDBDATABASE_METADATA,
  DELETE_DATABASE,
  TRANSACTION_COMMIT_METHOD,  // TRANSACTION_COMMIT is a WinNT.h macro
  GET_DATABASE_NAMES,
  DELETE_INDEX,
  CLEAR_OBJECT_STORE,
  INTERNAL_ERROR_MAX,
};

static void RecordInternalError(const char* type,
                                IndexedDBBackingStoreErrorSource location) {
  std::string name;
  name.append("WebCore.IndexedDB.BackingStore.").append(type).append("Error");
  base::Histogram::FactoryGet(name,
                              1,
                              INTERNAL_ERROR_MAX,
                              INTERNAL_ERROR_MAX + 1,
                              base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(location);
}

// Use to signal conditions caused by data corruption.
// A macro is used instead of an inline function so that the assert and log
// report the line number.
#define REPORT_ERROR(type, location)                      \
  do {                                                    \
    LOG(ERROR) << "IndexedDB " type " Error: " #location; \
    RecordInternalError(type, location);                  \
  } while (0)

#define INTERNAL_READ_ERROR(location) REPORT_ERROR("Read", location)
#define INTERNAL_CONSISTENCY_ERROR(location) \
  REPORT_ERROR("Consistency", location)
#define INTERNAL_WRITE_ERROR(location) REPORT_ERROR("Write", location)

// Use to signal conditions that usually indicate developer error, but
// could be caused by data corruption.  A macro is used instead of an
// inline function so that the assert and log report the line number.
// TODO: Improve test coverage so that all error conditions are "tested" and
//       then delete this macro.
#define REPORT_ERROR_UNTESTED(type, location)             \
  do {                                                    \
    LOG(ERROR) << "IndexedDB " type " Error: " #location; \
    NOTREACHED();                                         \
    RecordInternalError(type, location);                  \
  } while (0)

#define INTERNAL_READ_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Read", location)
#define INTERNAL_CONSISTENCY_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Consistency", location)
#define INTERNAL_WRITE_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Write", location)

static void PutBool(LevelDBTransaction* transaction,
                    const StringPiece& key,
                    bool value) {
  std::string buffer;
  EncodeBool(value, &buffer);
  transaction->Put(key, &buffer);
}

// Was able to use LevelDB to read the data w/o error, but the data read was not
// in the expected format.
static leveldb::Status InternalInconsistencyStatus() {
  return leveldb::Status::Corruption("Internal inconsistency");
}

static leveldb::Status InvalidDBKeyStatus() {
  return leveldb::Status::InvalidArgument("Invalid database key ID");
}

template <typename DBOrTransaction>
static leveldb::Status GetInt(DBOrTransaction* db,
                              const StringPiece& key,
                              int64* found_int,
                              bool* found) {
  std::string result;
  leveldb::Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return leveldb::Status::OK();
  StringPiece slice(result);
  if (DecodeInt(&slice, found_int) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

static void PutInt(LevelDBTransaction* transaction,
                   const StringPiece& key,
                   int64 value) {
  DCHECK_GE(value, 0);
  std::string buffer;
  EncodeInt(value, &buffer);
  transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
WARN_UNUSED_RESULT static leveldb::Status GetVarInt(DBOrTransaction* db,
                                                    const StringPiece& key,
                                                    int64* found_int,
                                                    bool* found) {
  std::string result;
  leveldb::Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return leveldb::Status::OK();
  StringPiece slice(result);
  if (DecodeVarInt(&slice, found_int) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

static void PutVarInt(LevelDBTransaction* transaction,
                      const StringPiece& key,
                      int64 value) {
  std::string buffer;
  EncodeVarInt(value, &buffer);
  transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
WARN_UNUSED_RESULT static leveldb::Status GetString(
    DBOrTransaction* db,
    const StringPiece& key,
    base::string16* found_string,
    bool* found) {
  std::string result;
  *found = false;
  leveldb::Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return leveldb::Status::OK();
  StringPiece slice(result);
  if (DecodeString(&slice, found_string) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

static void PutString(LevelDBTransaction* transaction,
                      const StringPiece& key,
                      const base::string16& value) {
  std::string buffer;
  EncodeString(value, &buffer);
  transaction->Put(key, &buffer);
}

static void PutIDBKeyPath(LevelDBTransaction* transaction,
                          const StringPiece& key,
                          const IndexedDBKeyPath& value) {
  std::string buffer;
  EncodeIDBKeyPath(value, &buffer);
  transaction->Put(key, &buffer);
}

static int CompareKeys(const StringPiece& a, const StringPiece& b) {
  return Compare(a, b, false /*index_keys*/);
}

static int CompareIndexKeys(const StringPiece& a, const StringPiece& b) {
  return Compare(a, b, true /*index_keys*/);
}

int IndexedDBBackingStore::Comparator::Compare(const StringPiece& a,
                                               const StringPiece& b) const {
  return content::Compare(a, b, false /*index_keys*/);
}

const char* IndexedDBBackingStore::Comparator::Name() const {
  return "idb_cmp1";
}

// 0 - Initial version.
// 1 - Adds UserIntVersion to DatabaseMetaData.
// 2 - Adds DataVersion to to global metadata.
static const int64 kLatestKnownSchemaVersion = 2;
WARN_UNUSED_RESULT static bool IsSchemaKnown(LevelDBDatabase* db, bool* known) {
  int64 db_schema_version = 0;
  bool found = false;
  leveldb::Status s =
      GetInt(db, SchemaVersionKey::Encode(), &db_schema_version, &found);
  if (!s.ok())
    return false;
  if (!found) {
    *known = true;
    return true;
  }
  if (db_schema_version > kLatestKnownSchemaVersion) {
    *known = false;
    return true;
  }

  const uint32 latest_known_data_version =
      blink::kSerializedScriptValueVersion;
  int64 db_data_version = 0;
  s = GetInt(db, DataVersionKey::Encode(), &db_data_version, &found);
  if (!s.ok())
    return false;
  if (!found) {
    *known = true;
    return true;
  }

  if (db_data_version > latest_known_data_version) {
    *known = false;
    return true;
  }

  *known = true;
  return true;
}

WARN_UNUSED_RESULT static bool SetUpMetadata(
    LevelDBDatabase* db,
    const std::string& origin_identifier) {
  const uint32 latest_known_data_version =
      blink::kSerializedScriptValueVersion;
  const std::string schema_version_key = SchemaVersionKey::Encode();
  const std::string data_version_key = DataVersionKey::Encode();

  scoped_refptr<LevelDBTransaction> transaction = new LevelDBTransaction(db);

  int64 db_schema_version = 0;
  int64 db_data_version = 0;
  bool found = false;
  leveldb::Status s =
      GetInt(transaction.get(), schema_version_key, &db_schema_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
    return false;
  }
  if (!found) {
    // Initialize new backing store.
    db_schema_version = kLatestKnownSchemaVersion;
    PutInt(transaction.get(), schema_version_key, db_schema_version);
    db_data_version = latest_known_data_version;
    PutInt(transaction.get(), data_version_key, db_data_version);
  } else {
    // Upgrade old backing store.
    DCHECK_LE(db_schema_version, kLatestKnownSchemaVersion);
    if (db_schema_version < 1) {
      db_schema_version = 1;
      PutInt(transaction.get(), schema_version_key, db_schema_version);
      const std::string start_key =
          DatabaseNameKey::EncodeMinKeyForOrigin(origin_identifier);
      const std::string stop_key =
          DatabaseNameKey::EncodeStopKeyForOrigin(origin_identifier);
      scoped_ptr<LevelDBIterator> it = db->CreateIterator();
      for (s = it->Seek(start_key);
           s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
           s = it->Next()) {
        int64 database_id = 0;
        found = false;
        s = GetInt(transaction.get(), it->Key(), &database_id, &found);
        if (!s.ok()) {
          INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
          return false;
        }
        if (!found) {
          INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_UP_METADATA);
          return false;
        }
        std::string int_version_key = DatabaseMetaDataKey::Encode(
            database_id, DatabaseMetaDataKey::USER_INT_VERSION);
        PutVarInt(transaction.get(),
                  int_version_key,
                  IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
      }
    }
    if (s.ok() && db_schema_version < 2) {
      db_schema_version = 2;
      PutInt(transaction.get(), schema_version_key, db_schema_version);
      db_data_version = blink::kSerializedScriptValueVersion;
      PutInt(transaction.get(), data_version_key, db_data_version);
    }
  }

  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
    return false;
  }

  // All new values will be written using this serialization version.
  found = false;
  s = GetInt(transaction.get(), data_version_key, &db_data_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
    return false;
  }
  if (!found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_UP_METADATA);
    return false;
  }
  if (db_data_version < latest_known_data_version) {
    db_data_version = latest_known_data_version;
    PutInt(transaction.get(), data_version_key, db_data_version);
  }

  DCHECK_EQ(db_schema_version, kLatestKnownSchemaVersion);
  DCHECK_EQ(db_data_version, latest_known_data_version);

  s = transaction->Commit();
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(SET_UP_METADATA);
    return false;
  }
  return true;
}

template <typename DBOrTransaction>
WARN_UNUSED_RESULT static leveldb::Status GetMaxObjectStoreId(
    DBOrTransaction* db,
    int64 database_id,
    int64* max_object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  return GetMaxObjectStoreId(db, max_object_store_id_key, max_object_store_id);
}

template <typename DBOrTransaction>
WARN_UNUSED_RESULT static leveldb::Status GetMaxObjectStoreId(
    DBOrTransaction* db,
    const std::string& max_object_store_id_key,
    int64* max_object_store_id) {
  *max_object_store_id = -1;
  bool found = false;
  leveldb::Status s =
      GetInt(db, max_object_store_id_key, max_object_store_id, &found);
  if (!s.ok())
    return s;
  if (!found)
    *max_object_store_id = 0;

  DCHECK_GE(*max_object_store_id, 0);
  return s;
}

class DefaultLevelDBFactory : public LevelDBFactory {
 public:
  virtual leveldb::Status OpenLevelDB(const base::FilePath& file_name,
                                      const LevelDBComparator* comparator,
                                      scoped_ptr<LevelDBDatabase>* db,
                                      bool* is_disk_full) OVERRIDE {
    return LevelDBDatabase::Open(file_name, comparator, db, is_disk_full);
  }
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& file_name)
      OVERRIDE {
    return LevelDBDatabase::Destroy(file_name);
  }
};

IndexedDBBackingStore::IndexedDBBackingStore(
    IndexedDBFactory* indexed_db_factory,
    const GURL& origin_url,
    const base::FilePath& blob_path,
    net::URLRequestContext* request_context,
    scoped_ptr<LevelDBDatabase> db,
    scoped_ptr<LevelDBComparator> comparator,
    base::TaskRunner* task_runner)
    : indexed_db_factory_(indexed_db_factory),
      origin_url_(origin_url),
      blob_path_(blob_path),
      origin_identifier_(ComputeOriginIdentifier(origin_url)),
      request_context_(request_context),
      task_runner_(task_runner),
      db_(db.Pass()),
      comparator_(comparator.Pass()),
      active_blob_registry_(this) {}

IndexedDBBackingStore::~IndexedDBBackingStore() {
  if (!blob_path_.empty() && !child_process_ids_granted_.empty()) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    std::set<int>::const_iterator iter;
    for (iter = child_process_ids_granted_.begin();
         iter != child_process_ids_granted_.end();
         ++iter) {
      policy->RevokeAllPermissionsForFile(*iter, blob_path_);
    }
  }
  // db_'s destructor uses comparator_. The order of destruction is important.
  db_.reset();
  comparator_.reset();
}

IndexedDBBackingStore::RecordIdentifier::RecordIdentifier(
    const std::string& primary_key,
    int64 version)
    : primary_key_(primary_key), version_(version) {
  DCHECK(!primary_key.empty());
}
IndexedDBBackingStore::RecordIdentifier::RecordIdentifier()
    : primary_key_(), version_(-1) {}
IndexedDBBackingStore::RecordIdentifier::~RecordIdentifier() {}

IndexedDBBackingStore::Cursor::CursorOptions::CursorOptions() {}
IndexedDBBackingStore::Cursor::CursorOptions::~CursorOptions() {}

enum IndexedDBBackingStoreOpenResult {
  INDEXED_DB_BACKING_STORE_OPEN_MEMORY_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_DESTROY_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_ERR,
  INDEXED_DB_BACKING_STORE_OPEN_MEMORY_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_ATTEMPT_NON_ASCII,
  INDEXED_DB_BACKING_STORE_OPEN_DISK_FULL_DEPRECATED,
  INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
  INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
  INDEXED_DB_BACKING_STORE_OPEN_MAX,
};

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::Open(
    IndexedDBFactory* indexed_db_factory,
    const GURL& origin_url,
    const base::FilePath& path_base,
    net::URLRequestContext* request_context,
    blink::WebIDBDataLoss* data_loss,
    std::string* data_loss_message,
    bool* disk_full,
    base::TaskRunner* task_runner,
    bool clean_journal) {
  *data_loss = blink::WebIDBDataLossNone;
  DefaultLevelDBFactory leveldb_factory;
  return IndexedDBBackingStore::Open(indexed_db_factory,
                                     origin_url,
                                     path_base,
                                     request_context,
                                     data_loss,
                                     data_loss_message,
                                     disk_full,
                                     &leveldb_factory,
                                     task_runner,
                                     clean_journal);
}

static std::string OriginToCustomHistogramSuffix(const GURL& origin_url) {
  if (origin_url.host() == "docs.google.com")
    return ".Docs";
  return std::string();
}

static void HistogramOpenStatus(IndexedDBBackingStoreOpenResult result,
                                const GURL& origin_url) {
  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.BackingStore.OpenStatus",
                            result,
                            INDEXED_DB_BACKING_STORE_OPEN_MAX);
  const std::string suffix = OriginToCustomHistogramSuffix(origin_url);
  // Data from the WebCore.IndexedDB.BackingStore.OpenStatus histogram is used
  // to generate a graph. So as not to alter the meaning of that graph,
  // continue to collect all stats there (above) but also now collect docs stats
  // separately (below).
  if (!suffix.empty()) {
    base::LinearHistogram::FactoryGet(
        "WebCore.IndexedDB.BackingStore.OpenStatus" + suffix,
        1,
        INDEXED_DB_BACKING_STORE_OPEN_MAX,
        INDEXED_DB_BACKING_STORE_OPEN_MAX + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)->Add(result);
  }
}

static bool IsPathTooLong(const base::FilePath& leveldb_dir) {
  int limit = base::GetMaximumPathComponentLength(leveldb_dir.DirName());
  if (limit == -1) {
    DLOG(WARNING) << "GetMaximumPathComponentLength returned -1";
    // In limited testing, ChromeOS returns 143, other OSes 255.
#if defined(OS_CHROMEOS)
    limit = 143;
#else
    limit = 255;
#endif
  }
  size_t component_length = leveldb_dir.BaseName().value().length();
  if (component_length > static_cast<uint32_t>(limit)) {
    DLOG(WARNING) << "Path component length (" << component_length
                  << ") exceeds maximum (" << limit
                  << ") allowed by this filesystem.";
    const int min = 140;
    const int max = 300;
    const int num_buckets = 12;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "WebCore.IndexedDB.BackingStore.OverlyLargeOriginLength",
        component_length,
        min,
        max,
        num_buckets);
    return true;
  }
  return false;
}

leveldb::Status IndexedDBBackingStore::DestroyBackingStore(
    const base::FilePath& path_base,
    const GURL& origin_url) {
  const base::FilePath file_path =
      path_base.Append(ComputeFileName(origin_url));
  DefaultLevelDBFactory leveldb_factory;
  return leveldb_factory.DestroyLevelDB(file_path);
}

bool IndexedDBBackingStore::ReadCorruptionInfo(const base::FilePath& path_base,
                                               const GURL& origin_url,
                                               std::string& message) {

  const base::FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(origin_url));

  if (IsPathTooLong(info_path))
    return false;

  const int64 max_json_len = 4096;
  int64 file_size(0);
  if (!GetFileSize(info_path, &file_size) || file_size > max_json_len)
    return false;
  if (!file_size) {
    NOTREACHED();
    return false;
  }

  base::File file(info_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  bool success = false;
  if (file.IsValid()) {
    std::vector<char> bytes(file_size);
    if (file_size == file.Read(0, &bytes[0], file_size)) {
      std::string input_js(&bytes[0], file_size);
      base::JSONReader reader;
      scoped_ptr<base::Value> val(reader.ReadToValue(input_js));
      if (val && val->GetType() == base::Value::TYPE_DICTIONARY) {
        base::DictionaryValue* dict_val =
            static_cast<base::DictionaryValue*>(val.get());
        success = dict_val->GetString("message", &message);
      }
    }
    file.Close();
  }

  base::DeleteFile(info_path, false);

  return success;
}

bool IndexedDBBackingStore::RecordCorruptionInfo(
    const base::FilePath& path_base,
    const GURL& origin_url,
    const std::string& message) {
  const base::FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(origin_url));
  if (IsPathTooLong(info_path))
    return false;

  base::DictionaryValue root_dict;
  root_dict.SetString("message", message);
  std::string output_js;
  base::JSONWriter::Write(&root_dict, &output_js);

  base::File file(info_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  int written = file.Write(0, output_js.c_str(), output_js.length());
  return size_t(written) == output_js.length();
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::Open(
    IndexedDBFactory* indexed_db_factory,
    const GURL& origin_url,
    const base::FilePath& path_base,
    net::URLRequestContext* request_context,
    blink::WebIDBDataLoss* data_loss,
    std::string* data_loss_message,
    bool* is_disk_full,
    LevelDBFactory* leveldb_factory,
    base::TaskRunner* task_runner,
    bool clean_journal) {
  IDB_TRACE("IndexedDBBackingStore::Open");
  DCHECK(!path_base.empty());
  *data_loss = blink::WebIDBDataLossNone;
  *data_loss_message = "";
  *is_disk_full = false;

  scoped_ptr<LevelDBComparator> comparator(new Comparator());

  if (!IsStringASCII(path_base.AsUTF8Unsafe())) {
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_ATTEMPT_NON_ASCII,
                        origin_url);
  }
  if (!base::CreateDirectory(path_base)) {
    LOG(ERROR) << "Unable to create IndexedDB database path "
               << path_base.AsUTF8Unsafe();
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                        origin_url);
    return scoped_refptr<IndexedDBBackingStore>();
  }

  const base::FilePath file_path =
      path_base.Append(ComputeFileName(origin_url));
  const base::FilePath blob_path =
      path_base.Append(ComputeBlobPath(origin_url));

  if (IsPathTooLong(file_path)) {
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                        origin_url);
    return scoped_refptr<IndexedDBBackingStore>();
  }

  scoped_ptr<LevelDBDatabase> db;
  leveldb::Status status = leveldb_factory->OpenLevelDB(
      file_path, comparator.get(), &db, is_disk_full);

  DCHECK(!db == !status.ok());
  if (!status.ok()) {
    if (leveldb_env::IndicatesDiskFull(status)) {
      *is_disk_full = true;
    } else if (leveldb_env::IsCorruption(status)) {
      *data_loss = blink::WebIDBDataLossTotal;
      *data_loss_message = leveldb_env::GetCorruptionMessage(status);
    }
  }

  bool is_schema_known = false;
  if (db) {
    std::string corruption_message;
    if (ReadCorruptionInfo(path_base, origin_url, corruption_message)) {
      LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                    "database.";
      HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
                          origin_url);
      db.reset();
      *data_loss = blink::WebIDBDataLossTotal;
      *data_loss_message =
          "IndexedDB (database was corrupt): " + corruption_message;
    } else if (!IsSchemaKnown(db.get(), &is_schema_known)) {
      LOG(ERROR) << "IndexedDB had IO error checking schema, treating it as "
                    "failure to open";
      HistogramOpenStatus(
          INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
          origin_url);
      db.reset();
      *data_loss = blink::WebIDBDataLossTotal;
      *data_loss_message = "I/O error checking schema";
    } else if (!is_schema_known) {
      LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it "
                    "as failure to open";
      HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
                          origin_url);
      db.reset();
      *data_loss = blink::WebIDBDataLossTotal;
      *data_loss_message = "Unknown schema";
    }
  }

  DCHECK(status.ok() || !is_schema_known || leveldb_env::IsIOError(status) ||
         leveldb_env::IsCorruption(status));

  if (db) {
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_SUCCESS, origin_url);
  } else if (leveldb_env::IsIOError(status)) {
    LOG(ERROR) << "Unable to open backing store, not trying to recover - "
               << status.ToString();
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY, origin_url);
    return scoped_refptr<IndexedDBBackingStore>();
  } else {
    DCHECK(!is_schema_known || leveldb_env::IsCorruption(status));
    LOG(ERROR) << "IndexedDB backing store open failed, attempting cleanup";
    status = leveldb_factory->DestroyLevelDB(file_path);
    if (!status.ok()) {
      LOG(ERROR) << "IndexedDB backing store cleanup failed";
      HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_DESTROY_FAILED,
                          origin_url);
      return scoped_refptr<IndexedDBBackingStore>();
    }

    LOG(ERROR) << "IndexedDB backing store cleanup succeeded, reopening";
    leveldb_factory->OpenLevelDB(file_path, comparator.get(), &db, NULL);
    if (!db) {
      LOG(ERROR) << "IndexedDB backing store reopen after recovery failed";
      HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_FAILED,
                          origin_url);
      return scoped_refptr<IndexedDBBackingStore>();
    }
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_SUCCESS,
                        origin_url);
  }

  if (!db) {
    NOTREACHED();
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_ERR,
                        origin_url);
    return scoped_refptr<IndexedDBBackingStore>();
  }

  return Create(indexed_db_factory,
                origin_url,
                blob_path,
                request_context,
                db.Pass(),
                comparator.Pass(),
                task_runner);
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::OpenInMemory(
    const GURL& origin_url,
    base::TaskRunner* task_runner) {
  DefaultLevelDBFactory leveldb_factory;
  return IndexedDBBackingStore::OpenInMemory(
      origin_url, &leveldb_factory, task_runner);
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::OpenInMemory(
    const GURL& origin_url,
    LevelDBFactory* leveldb_factory,
    base::TaskRunner* task_runner) {
  IDB_TRACE("IndexedDBBackingStore::OpenInMemory");

  scoped_ptr<LevelDBComparator> comparator(new Comparator());
  scoped_ptr<LevelDBDatabase> db =
      LevelDBDatabase::OpenInMemory(comparator.get());
  if (!db) {
    LOG(ERROR) << "LevelDBDatabase::OpenInMemory failed.";
    HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_MEMORY_FAILED,
                        origin_url);
    return scoped_refptr<IndexedDBBackingStore>();
  }
  HistogramOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_MEMORY_SUCCESS, origin_url);

  return Create(NULL /* indexed_db_factory */,
                origin_url,
                base::FilePath(),
                NULL /* request_context */,
                db.Pass(),
                comparator.Pass(),
                task_runner);
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::Create(
    IndexedDBFactory* indexed_db_factory,
    const GURL& origin_url,
    const base::FilePath& blob_path,
    net::URLRequestContext* request_context,
    scoped_ptr<LevelDBDatabase> db,
    scoped_ptr<LevelDBComparator> comparator,
    base::TaskRunner* task_runner) {
  // TODO(jsbell): Handle comparator name changes.
  scoped_refptr<IndexedDBBackingStore> backing_store(
      new IndexedDBBackingStore(indexed_db_factory,
                                origin_url,
                                blob_path,
                                request_context,
                                db.Pass(),
                                comparator.Pass(),
                                task_runner));
  if (!SetUpMetadata(backing_store->db_.get(),
                     backing_store->origin_identifier_))
    return scoped_refptr<IndexedDBBackingStore>();

  return backing_store;
}

void IndexedDBBackingStore::GrantChildProcessPermissions(int child_process_id) {
  if (!child_process_ids_granted_.count(child_process_id)) {
    child_process_ids_granted_.insert(child_process_id);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        child_process_id, blob_path_);
  }
}

std::vector<base::string16> IndexedDBBackingStore::GetDatabaseNames(
    leveldb::Status* s) {
  *s = leveldb::Status::OK();
  std::vector<base::string16> found_names;
  const std::string start_key =
      DatabaseNameKey::EncodeMinKeyForOrigin(origin_identifier_);
  const std::string stop_key =
      DatabaseNameKey::EncodeStopKeyForOrigin(origin_identifier_);

  DCHECK(found_names.empty());

  scoped_ptr<LevelDBIterator> it = db_->CreateIterator();
  for (*s = it->Seek(start_key);
       s->ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       *s = it->Next()) {
    StringPiece slice(it->Key());
    DatabaseNameKey database_name_key;
    if (!DatabaseNameKey::Decode(&slice, &database_name_key)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_DATABASE_NAMES);
      continue;
    }
    found_names.push_back(database_name_key.database_name());
  }

  if (!s->ok())
    INTERNAL_READ_ERROR_UNTESTED(GET_DATABASE_NAMES);

  return found_names;
}

leveldb::Status IndexedDBBackingStore::GetIDBDatabaseMetaData(
    const base::string16& name,
    IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  const std::string key = DatabaseNameKey::Encode(origin_identifier_, name);
  *found = false;

  leveldb::Status s = GetInt(db_.get(), key, &metadata->id, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found)
    return leveldb::Status::OK();

  s = GetString(db_.get(),
                DatabaseMetaDataKey::Encode(metadata->id,
                                            DatabaseMetaDataKey::USER_VERSION),
                &metadata->version,
                found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return InternalInconsistencyStatus();
  }

  s = GetVarInt(db_.get(),
                DatabaseMetaDataKey::Encode(
                    metadata->id, DatabaseMetaDataKey::USER_INT_VERSION),
                &metadata->int_version,
                found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return InternalInconsistencyStatus();
  }

  if (metadata->int_version == IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION)
    metadata->int_version = IndexedDBDatabaseMetadata::NO_INT_VERSION;

  s = GetMaxObjectStoreId(
      db_.get(), metadata->id, &metadata->max_object_store_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
  }

  return s;
}

WARN_UNUSED_RESULT static leveldb::Status GetNewDatabaseId(
    LevelDBTransaction* transaction,
    int64* new_id) {
  *new_id = -1;
  int64 max_database_id = -1;
  bool found = false;
  leveldb::Status s =
      GetInt(transaction, MaxDatabaseIdKey::Encode(), &max_database_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_DATABASE_ID);
    return s;
  }
  if (!found)
    max_database_id = 0;

  DCHECK_GE(max_database_id, 0);

  int64 database_id = max_database_id + 1;
  PutInt(transaction, MaxDatabaseIdKey::Encode(), database_id);
  *new_id = database_id;
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBBackingStore::CreateIDBDatabaseMetaData(
    const base::string16& name,
    const base::string16& version,
    int64 int_version,
    int64* row_id) {
  scoped_refptr<LevelDBTransaction> transaction =
      new LevelDBTransaction(db_.get());

  leveldb::Status s = GetNewDatabaseId(transaction.get(), row_id);
  if (!s.ok())
    return s;
  DCHECK_GE(*row_id, 0);

  if (int_version == IndexedDBDatabaseMetadata::NO_INT_VERSION)
    int_version = IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION;

  PutInt(transaction.get(),
         DatabaseNameKey::Encode(origin_identifier_, name),
         *row_id);
  PutString(
      transaction.get(),
      DatabaseMetaDataKey::Encode(*row_id, DatabaseMetaDataKey::USER_VERSION),
      version);
  PutVarInt(transaction.get(),
            DatabaseMetaDataKey::Encode(*row_id,
                                        DatabaseMetaDataKey::USER_INT_VERSION),
            int_version);
  s = transaction->Commit();
  if (!s.ok())
    INTERNAL_WRITE_ERROR_UNTESTED(CREATE_IDBDATABASE_METADATA);
  return s;
}

bool IndexedDBBackingStore::UpdateIDBDatabaseIntVersion(
    IndexedDBBackingStore::Transaction* transaction,
    int64 row_id,
    int64 int_version) {
  if (int_version == IndexedDBDatabaseMetadata::NO_INT_VERSION)
    int_version = IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION;
  DCHECK_GE(int_version, 0) << "int_version was " << int_version;
  PutVarInt(transaction->transaction(),
            DatabaseMetaDataKey::Encode(row_id,
                                        DatabaseMetaDataKey::USER_INT_VERSION),
            int_version);
  return true;
}

static leveldb::Status DeleteRange(LevelDBTransaction* transaction,
                                   const std::string& begin,
                                   const std::string& end) {
  scoped_ptr<LevelDBIterator> it = transaction->CreateIterator();
  leveldb::Status s;
  for (s = it->Seek(begin);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), end) < 0;
       s = it->Next())
    transaction->Remove(it->Key());
  return s;
}

leveldb::Status IndexedDBBackingStore::DeleteDatabase(
    const base::string16& name) {
  IDB_TRACE("IndexedDBBackingStore::DeleteDatabase");
  scoped_ptr<LevelDBDirectTransaction> transaction =
      LevelDBDirectTransaction::Create(db_.get());

  IndexedDBDatabaseMetadata metadata;
  bool success = false;
  leveldb::Status s = GetIDBDatabaseMetaData(name, &metadata, &success);
  if (!s.ok())
    return s;
  if (!success)
    return leveldb::Status::OK();

  const std::string start_key = DatabaseMetaDataKey::Encode(
      metadata.id, DatabaseMetaDataKey::ORIGIN_NAME);
  const std::string stop_key = DatabaseMetaDataKey::Encode(
      metadata.id + 1, DatabaseMetaDataKey::ORIGIN_NAME);
  scoped_ptr<LevelDBIterator> it = db_->CreateIterator();
  for (s = it->Seek(start_key);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       s = it->Next())
    transaction->Remove(it->Key());
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_DATABASE);
    return s;
  }

  const std::string key = DatabaseNameKey::Encode(origin_identifier_, name);
  transaction->Remove(key);

  s = transaction->Commit();
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_DATABASE);
    return s;
  }
  db_->Compact(start_key, stop_key);
  return s;
}

static bool CheckObjectStoreAndMetaDataType(const LevelDBIterator* it,
                                            const std::string& stop_key,
                                            int64 object_store_id,
                                            int64 meta_data_type) {
  if (!it->IsValid() || CompareKeys(it->Key(), stop_key) >= 0)
    return false;

  StringPiece slice(it->Key());
  ObjectStoreMetaDataKey meta_data_key;
  bool ok = ObjectStoreMetaDataKey::Decode(&slice, &meta_data_key);
  DCHECK(ok);
  if (meta_data_key.ObjectStoreId() != object_store_id)
    return false;
  if (meta_data_key.MetaDataType() != meta_data_type)
    return false;
  return true;
}

// TODO(jsbell): This should do some error handling rather than
// plowing ahead when bad data is encountered.
leveldb::Status IndexedDBBackingStore::GetObjectStores(
    int64 database_id,
    IndexedDBDatabaseMetadata::ObjectStoreMap* object_stores) {
  IDB_TRACE("IndexedDBBackingStore::GetObjectStores");
  if (!KeyPrefix::IsValidDatabaseId(database_id))
    return InvalidDBKeyStatus();
  const std::string start_key =
      ObjectStoreMetaDataKey::Encode(database_id, 1, 0);
  const std::string stop_key =
      ObjectStoreMetaDataKey::EncodeMaxKey(database_id);

  DCHECK(object_stores->empty());

  scoped_ptr<LevelDBIterator> it = db_->CreateIterator();
  leveldb::Status s = it->Seek(start_key);
  while (s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0) {
    StringPiece slice(it->Key());
    ObjectStoreMetaDataKey meta_data_key;
    bool ok = ObjectStoreMetaDataKey::Decode(&slice, &meta_data_key);
    DCHECK(ok);
    if (meta_data_key.MetaDataType() != ObjectStoreMetaDataKey::NAME) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      // Possible stale metadata, but don't fail the load.
      s = it->Next();
      if (!s.ok())
        break;
      continue;
    }

    int64 object_store_id = meta_data_key.ObjectStoreId();

    // TODO(jsbell): Do this by direct key lookup rather than iteration, to
    // simplify.
    base::string16 object_store_name;
    {
      StringPiece slice(it->Value());
      if (!DecodeString(&slice, &object_store_name) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(it.get(),
                                         stop_key,
                                         object_store_id,
                                         ObjectStoreMetaDataKey::KEY_PATH)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }
    IndexedDBKeyPath key_path;
    {
      StringPiece slice(it->Value());
      if (!DecodeIDBKeyPath(&slice, &key_path) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(
             it.get(),
             stop_key,
             object_store_id,
             ObjectStoreMetaDataKey::AUTO_INCREMENT)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }
    bool auto_increment;
    {
      StringPiece slice(it->Value());
      if (!DecodeBool(&slice, &auto_increment) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();  // Is evictable.
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(it.get(),
                                         stop_key,
                                         object_store_id,
                                         ObjectStoreMetaDataKey::EVICTABLE)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }

    s = it->Next();  // Last version.
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(
             it.get(),
             stop_key,
             object_store_id,
             ObjectStoreMetaDataKey::LAST_VERSION)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }

    s = it->Next();  // Maximum index id allocated.
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(
             it.get(),
             stop_key,
             object_store_id,
             ObjectStoreMetaDataKey::MAX_INDEX_ID)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }
    int64 max_index_id;
    {
      StringPiece slice(it->Value());
      if (!DecodeInt(&slice, &max_index_id) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();  // [optional] has key path (is not null)
    if (!s.ok())
      break;
    if (CheckObjectStoreAndMetaDataType(it.get(),
                                        stop_key,
                                        object_store_id,
                                        ObjectStoreMetaDataKey::HAS_KEY_PATH)) {
      bool has_key_path;
      {
        StringPiece slice(it->Value());
        if (!DecodeBool(&slice, &has_key_path))
          INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      }
      // This check accounts for two layers of legacy coding:
      // (1) Initially, has_key_path was added to distinguish null vs. string.
      // (2) Later, null vs. string vs. array was stored in the key_path itself.
      // So this check is only relevant for string-type key_paths.
      if (!has_key_path &&
          (key_path.type() == blink::WebIDBKeyPathTypeString &&
           !key_path.string().empty())) {
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
        break;
      }
      if (!has_key_path)
        key_path = IndexedDBKeyPath();
      s = it->Next();
      if (!s.ok())
        break;
    }

    int64 key_generator_current_number = -1;
    if (CheckObjectStoreAndMetaDataType(
            it.get(),
            stop_key,
            object_store_id,
            ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER)) {
      StringPiece slice(it->Value());
      if (!DecodeInt(&slice, &key_generator_current_number) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);

      // TODO(jsbell): Return key_generator_current_number, cache in
      // object store, and write lazily to backing store.  For now,
      // just assert that if it was written it was valid.
      DCHECK_GE(key_generator_current_number, kKeyGeneratorInitialNumber);
      s = it->Next();
      if (!s.ok())
        break;
    }

    IndexedDBObjectStoreMetadata metadata(object_store_name,
                                          object_store_id,
                                          key_path,
                                          auto_increment,
                                          max_index_id);
    s = GetIndexes(database_id, object_store_id, &metadata.indexes);
    if (!s.ok())
      break;
    (*object_stores)[object_store_id] = metadata;
  }

  if (!s.ok())
    INTERNAL_READ_ERROR_UNTESTED(GET_OBJECT_STORES);

  return s;
}

WARN_UNUSED_RESULT static leveldb::Status SetMaxObjectStoreId(
    LevelDBTransaction* transaction,
    int64 database_id,
    int64 object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  int64 max_object_store_id = -1;
  leveldb::Status s = GetMaxObjectStoreId(
      transaction, max_object_store_id_key, &max_object_store_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_MAX_OBJECT_STORE_ID);
    return s;
  }

  if (object_store_id <= max_object_store_id) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_MAX_OBJECT_STORE_ID);
    return InternalInconsistencyStatus();
  }
  PutInt(transaction, max_object_store_id_key, object_store_id);
  return s;
}

void IndexedDBBackingStore::Compact() { db_->CompactAll(); }

leveldb::Status IndexedDBBackingStore::CreateObjectStore(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const base::string16& name,
    const IndexedDBKeyPath& key_path,
    bool auto_increment) {
  IDB_TRACE("IndexedDBBackingStore::CreateObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  leveldb::Status s =
      SetMaxObjectStoreId(leveldb_transaction, database_id, object_store_id);
  if (!s.ok())
    return s;

  const std::string name_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::NAME);
  const std::string key_path_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::KEY_PATH);
  const std::string auto_increment_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::AUTO_INCREMENT);
  const std::string evictable_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::EVICTABLE);
  const std::string last_version_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::LAST_VERSION);
  const std::string max_index_id_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::MAX_INDEX_ID);
  const std::string has_key_path_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::HAS_KEY_PATH);
  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id,
          object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);
  const std::string names_key = ObjectStoreNamesKey::Encode(database_id, name);

  PutString(leveldb_transaction, name_key, name);
  PutIDBKeyPath(leveldb_transaction, key_path_key, key_path);
  PutInt(leveldb_transaction, auto_increment_key, auto_increment);
  PutInt(leveldb_transaction, evictable_key, false);
  PutInt(leveldb_transaction, last_version_key, 1);
  PutInt(leveldb_transaction, max_index_id_key, kMinimumIndexId);
  PutBool(leveldb_transaction, has_key_path_key, !key_path.IsNull());
  PutInt(leveldb_transaction,
         key_generator_current_number_key,
         kKeyGeneratorInitialNumber);
  PutInt(leveldb_transaction, names_key, object_store_id);
  return s;
}

leveldb::Status IndexedDBBackingStore::DeleteObjectStore(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id) {
  IDB_TRACE("IndexedDBBackingStore::DeleteObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  base::string16 object_store_name;
  bool found = false;
  leveldb::Status s =
      GetString(leveldb_transaction,
                ObjectStoreMetaDataKey::Encode(
                    database_id, object_store_id, ObjectStoreMetaDataKey::NAME),
                &object_store_name,
                &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }

  s = DeleteRange(
      leveldb_transaction,
      ObjectStoreMetaDataKey::Encode(database_id, object_store_id, 0),
      ObjectStoreMetaDataKey::EncodeMaxKey(database_id, object_store_id));

  if (s.ok()) {
    leveldb_transaction->Remove(
        ObjectStoreNamesKey::Encode(database_id, object_store_name));

    s = DeleteRange(
        leveldb_transaction,
        IndexFreeListKey::Encode(database_id, object_store_id, 0),
        IndexFreeListKey::EncodeMaxKey(database_id, object_store_id));
  }

  if (s.ok()) {
    s = DeleteRange(
        leveldb_transaction,
        IndexMetaDataKey::Encode(database_id, object_store_id, 0, 0),
        IndexMetaDataKey::EncodeMaxKey(database_id, object_store_id));
  }

  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return s;
  }

  return ClearObjectStore(transaction, database_id, object_store_id);
}

leveldb::Status IndexedDBBackingStore::GetRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue* record) {
  IDB_TRACE("IndexedDBBackingStore::GetRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string leveldb_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);
  std::string data;

  record->clear();

  bool found = false;
  leveldb::Status s = leveldb_transaction->Get(leveldb_key, &data, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_RECORD);
    return s;
  }
  if (!found)
    return s;
  if (data.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_RECORD);
    return leveldb::Status::NotFound("Record contained no data");
  }

  int64 version;
  StringPiece slice(data);
  if (!DecodeVarInt(&slice, &version)) {
    INTERNAL_READ_ERROR_UNTESTED(GET_RECORD);
    return InternalInconsistencyStatus();
  }

  record->bits = slice.as_string();
  return s;
}

WARN_UNUSED_RESULT static leveldb::Status GetNewVersionNumber(
    LevelDBTransaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64* new_version_number) {
  const std::string last_version_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::LAST_VERSION);

  *new_version_number = -1;
  int64 last_version = -1;
  bool found = false;
  leveldb::Status s =
      GetInt(transaction, last_version_key, &last_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_VERSION_NUMBER);
    return s;
  }
  if (!found)
    last_version = 0;

  DCHECK_GE(last_version, 0);

  int64 version = last_version + 1;
  PutInt(transaction, last_version_key, version);

  // TODO(jsbell): Think about how we want to handle the overflow scenario.
  DCHECK(version > last_version);

  *new_version_number = version;
  return s;
}

leveldb::Status IndexedDBBackingStore::PutRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue& value,
    ScopedVector<webkit_blob::BlobDataHandle>* handles,
    RecordIdentifier* record_identifier) {
  IDB_TRACE("IndexedDBBackingStore::PutRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  DCHECK(key.IsValid());

  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  int64 version = -1;
  leveldb::Status s = GetNewVersionNumber(
      leveldb_transaction, database_id, object_store_id, &version);
  if (!s.ok())
    return s;
  DCHECK_GE(version, 0);
  const std::string object_store_data_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);

  std::string v;
  EncodeVarInt(version, &v);
  v.append(value.bits);

  leveldb_transaction->Put(object_store_data_key, &v);
  transaction->PutBlobInfo(database_id,
                           object_store_id,
                           object_store_data_key,
                           &value.blob_info,
                           handles);
  DCHECK(!handles->size());

  const std::string exists_entry_key =
      ExistsEntryKey::Encode(database_id, object_store_id, key);
  std::string version_encoded;
  EncodeInt(version, &version_encoded);
  leveldb_transaction->Put(exists_entry_key, &version_encoded);

  std::string key_encoded;
  EncodeIDBKey(key, &key_encoded);
  record_identifier->Reset(key_encoded, version);
  return s;
}

leveldb::Status IndexedDBBackingStore::ClearObjectStore(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id) {
  IDB_TRACE("IndexedDBBackingStore::ClearObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  const std::string start_key =
      KeyPrefix(database_id, object_store_id).Encode();
  const std::string stop_key =
      KeyPrefix(database_id, object_store_id + 1).Encode();

  leveldb::Status s =
      DeleteRange(transaction->transaction(), start_key, stop_key);
  if (!s.ok())
    INTERNAL_WRITE_ERROR(CLEAR_OBJECT_STORE);
  return s;
}

leveldb::Status IndexedDBBackingStore::DeleteRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const RecordIdentifier& record_identifier) {
  IDB_TRACE("IndexedDBBackingStore::DeleteRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string object_store_data_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, record_identifier.primary_key());
  leveldb_transaction->Remove(object_store_data_key);
  transaction->PutBlobInfo(
      database_id, object_store_id, object_store_data_key, NULL, NULL);

  const std::string exists_entry_key = ExistsEntryKey::Encode(
      database_id, object_store_id, record_identifier.primary_key());
  leveldb_transaction->Remove(exists_entry_key);
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBBackingStore::GetKeyGeneratorCurrentNumber(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64* key_generator_current_number) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id,
          object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);

  *key_generator_current_number = -1;
  std::string data;

  bool found = false;
  leveldb::Status s =
      leveldb_transaction->Get(key_generator_current_number_key, &data, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);
    return s;
  }
  if (found && !data.empty()) {
    StringPiece slice(data);
    if (!DecodeInt(&slice, key_generator_current_number) || !slice.empty()) {
      INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);
      return InternalInconsistencyStatus();
    }
    return s;
  }

  // Previously, the key generator state was not stored explicitly
  // but derived from the maximum numeric key present in existing
  // data. This violates the spec as the data may be cleared but the
  // key generator state must be preserved.
  // TODO(jsbell): Fix this for all stores on database open?
  const std::string start_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, MinIDBKey());
  const std::string stop_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, MaxIDBKey());

  scoped_ptr<LevelDBIterator> it = leveldb_transaction->CreateIterator();
  int64 max_numeric_key = 0;

  for (s = it->Seek(start_key);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       s = it->Next()) {
    StringPiece slice(it->Key());
    ObjectStoreDataKey data_key;
    if (!ObjectStoreDataKey::Decode(&slice, &data_key)) {
      INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);
      return InternalInconsistencyStatus();
    }
    scoped_ptr<IndexedDBKey> user_key = data_key.user_key();
    if (user_key->type() == blink::WebIDBKeyTypeNumber) {
      int64 n = static_cast<int64>(user_key->number());
      if (n > max_numeric_key)
        max_numeric_key = n;
    }
  }

  if (s.ok())
    *key_generator_current_number = max_numeric_key + 1;
  else
    INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);

  return s;
}

leveldb::Status IndexedDBBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 new_number,
    bool check_current) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();

  if (check_current) {
    int64 current_number;
    leveldb::Status s = GetKeyGeneratorCurrentNumber(
        transaction, database_id, object_store_id, &current_number);
    if (!s.ok())
      return s;
    if (new_number <= current_number)
      return s;
  }

  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id,
          object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);
  PutInt(
      transaction->transaction(), key_generator_current_number_key, new_number);
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBBackingStore::KeyExistsInObjectStore(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey& key,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  IDB_TRACE("IndexedDBBackingStore::KeyExistsInObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  *found = false;
  const std::string leveldb_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);
  std::string data;

  leveldb::Status s =
      transaction->transaction()->Get(leveldb_key, &data, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_OBJECT_STORE);
    return s;
  }
  if (!*found)
    return leveldb::Status::OK();
  if (!data.size()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }

  int64 version;
  StringPiece slice(data);
  if (!DecodeVarInt(&slice, &version))
    return InternalInconsistencyStatus();

  std::string encoded_key;
  EncodeIDBKey(key, &encoded_key);
  found_record_identifier->Reset(encoded_key, version);
  return s;
}

class IndexedDBBackingStore::Transaction::ChainedBlobWriterImpl
    : public IndexedDBBackingStore::Transaction::ChainedBlobWriter {
 public:
  typedef IndexedDBBackingStore::Transaction::WriteDescriptorVec
      WriteDescriptorVec;
  ChainedBlobWriterImpl(
      int64 database_id,
      IndexedDBBackingStore* backingStore,
      WriteDescriptorVec& blobs,
      scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback)
      : waiting_for_callback_(false),
        database_id_(database_id),
        backing_store_(backingStore),
        callback_(callback),
        aborted_(false) {
    blobs_.swap(blobs);
    iter_ = blobs_.begin();
    WriteNextFile();
  }

  virtual void set_delegate(scoped_ptr<FileWriterDelegate> delegate) OVERRIDE {
    delegate_.reset(delegate.release());
  }

  virtual void ReportWriteCompletion(bool succeeded,
                                     int64 bytes_written) OVERRIDE {
    // TODO(ericu): Check bytes_written against the blob's snapshot value.
    DCHECK(waiting_for_callback_);
    DCHECK(!succeeded || bytes_written >= 0);
    waiting_for_callback_ = false;
    if (delegate_.get())  // Only present for Blob, not File.
      content::BrowserThread::DeleteSoon(
          content::BrowserThread::IO, FROM_HERE, delegate_.release());
    if (aborted_) {
      self_ref_ = NULL;
      return;
    }
    if (succeeded)
      WriteNextFile();
    else
      callback_->Run(false);
  }

  virtual void Abort() OVERRIDE {
    if (!waiting_for_callback_)
      return;
    self_ref_ = this;
    aborted_ = true;
  }

 private:
  virtual ~ChainedBlobWriterImpl() {}

  void WriteNextFile() {
    DCHECK(!waiting_for_callback_);
    DCHECK(!aborted_);
    if (iter_ == blobs_.end()) {
      DCHECK(!self_ref_);
      callback_->Run(true);
      return;
    } else {
      if (!backing_store_->WriteBlobFile(database_id_, *iter_, this)) {
        callback_->Run(false);
        return;
      }
      waiting_for_callback_ = true;
      ++iter_;
    }
  }

  bool waiting_for_callback_;
  scoped_refptr<ChainedBlobWriterImpl> self_ref_;
  WriteDescriptorVec blobs_;
  WriteDescriptorVec::const_iterator iter_;
  int64 database_id_;
  IndexedDBBackingStore* backing_store_;
  scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback_;
  scoped_ptr<FileWriterDelegate> delegate_;
  bool aborted_;
};

class LocalWriteClosure : public FileWriterDelegate::DelegateWriteCallback,
                          public base::RefCounted<LocalWriteClosure> {
 public:
  LocalWriteClosure(IndexedDBBackingStore::Transaction::ChainedBlobWriter*
                        chained_blob_writer_,
                    base::TaskRunner* task_runner)
      : chained_blob_writer_(chained_blob_writer_),
        task_runner_(task_runner),
        bytes_written_(-1) {}

  void Run(base::File::Error rv,
           int64 bytes,
           FileWriterDelegate::WriteProgressStatus write_status) {
    if (write_status == FileWriterDelegate::SUCCESS_IO_PENDING)
      return;  // We don't care about progress events.
    if (rv == base::File::FILE_OK) {
      DCHECK(bytes >= 0);
      DCHECK(write_status == FileWriterDelegate::SUCCESS_COMPLETED);
      bytes_written_ = bytes;
    } else {
      DCHECK(write_status == FileWriterDelegate::ERROR_WRITE_STARTED ||
             write_status == FileWriterDelegate::ERROR_WRITE_NOT_STARTED);
    }
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalWriteClosure::callBlobCallbackOnIDBTaskRunner,
                   this,
                   write_status == FileWriterDelegate::SUCCESS_COMPLETED));
  }

  void writeBlobToFileOnIOThread(const FilePath& file_path,
                                 const GURL& blob_url,
                                 net::URLRequestContext* request_context) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    scoped_ptr<fileapi::FileStreamWriter> writer(
        fileapi::FileStreamWriter::CreateForLocalFile(
            task_runner_, file_path, 0,
            fileapi::FileStreamWriter::CREATE_NEW_FILE));
    scoped_ptr<FileWriterDelegate> delegate(
        new FileWriterDelegate(writer.Pass()));

    DCHECK(blob_url.is_valid());
    scoped_ptr<net::URLRequest> blob_request(request_context->CreateRequest(
        blob_url, net::DEFAULT_PRIORITY, delegate.get(), NULL));

    delegate->Start(blob_request.Pass(),
                    base::Bind(&LocalWriteClosure::Run, this));
    chained_blob_writer_->set_delegate(delegate.Pass());
  }

 private:
  virtual ~LocalWriteClosure() {}
  friend class base::RefCounted<LocalWriteClosure>;

  void callBlobCallbackOnIDBTaskRunner(bool succeeded) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    chained_blob_writer_->ReportWriteCompletion(succeeded, bytes_written_);
  }

  IndexedDBBackingStore::Transaction::ChainedBlobWriter* chained_blob_writer_;
  base::TaskRunner* task_runner_;
  int64 bytes_written_;
};

bool IndexedDBBackingStore::WriteBlobFile(
    int64 database_id,
    const Transaction::WriteDescriptor& descriptor,
    Transaction::ChainedBlobWriter* chained_blob_writer) {

  if (!MakeIDBBlobDirectory(blob_path_, database_id, descriptor.key()))
    return false;

  FilePath path = GetBlobFileName(database_id, descriptor.key());

  if (descriptor.is_file()) {
    DCHECK(!descriptor.file_path().empty());
    if (!base::CopyFile(descriptor.file_path(), path))
      return false;

    base::File::Info info;
    if (base::GetFileInfo(descriptor.file_path(), &info)) {
      // TODO(ericu): Validate the snapshot date here.  Expand WriteDescriptor
      // to include snapshot date and file size, and check both.
      if (!base::TouchFile(path, info.last_accessed, info.last_modified)) {
        // TODO(ericu): Complain quietly; timestamp's probably not vital.
      }
    } else {
      // TODO(ericu): Complain quietly; timestamp's probably not vital.
    }

    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Transaction::ChainedBlobWriter::ReportWriteCompletion,
                   chained_blob_writer,
                   true,
                   info.size));
  } else {
    DCHECK(descriptor.url().is_valid());
    scoped_refptr<LocalWriteClosure> write_closure(
        new LocalWriteClosure(chained_blob_writer, task_runner_));
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&LocalWriteClosure::writeBlobToFileOnIOThread,
                   write_closure.get(),
                   path,
                   descriptor.url(),
                   request_context_));
  }
  return true;
}

// This assumes a file path of dbId/second-to-LSB-of-counter/counter.
FilePath IndexedDBBackingStore::GetBlobFileName(int64 database_id, int64 key) {
  return GetBlobFileNameForKey(blob_path_, database_id, key);
}

static bool CheckIndexAndMetaDataKey(const LevelDBIterator* it,
                                     const std::string& stop_key,
                                     int64 index_id,
                                     unsigned char meta_data_type) {
  if (!it->IsValid() || CompareKeys(it->Key(), stop_key) >= 0)
    return false;

  StringPiece slice(it->Key());
  IndexMetaDataKey meta_data_key;
  bool ok = IndexMetaDataKey::Decode(&slice, &meta_data_key);
  DCHECK(ok);
  if (meta_data_key.IndexId() != index_id)
    return false;
  if (meta_data_key.meta_data_type() != meta_data_type)
    return false;
  return true;
}

// TODO(jsbell): This should do some error handling rather than plowing ahead
// when bad data is encountered.
leveldb::Status IndexedDBBackingStore::GetIndexes(
    int64 database_id,
    int64 object_store_id,
    IndexedDBObjectStoreMetadata::IndexMap* indexes) {
  IDB_TRACE("IndexedDBBackingStore::GetIndexes");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  const std::string start_key =
      IndexMetaDataKey::Encode(database_id, object_store_id, 0, 0);
  const std::string stop_key =
      IndexMetaDataKey::Encode(database_id, object_store_id + 1, 0, 0);

  DCHECK(indexes->empty());

  scoped_ptr<LevelDBIterator> it = db_->CreateIterator();
  leveldb::Status s = it->Seek(start_key);
  while (s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0) {
    StringPiece slice(it->Key());
    IndexMetaDataKey meta_data_key;
    bool ok = IndexMetaDataKey::Decode(&slice, &meta_data_key);
    DCHECK(ok);
    if (meta_data_key.meta_data_type() != IndexMetaDataKey::NAME) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
      // Possible stale metadata due to http://webkit.org/b/85557 but don't fail
      // the load.
      s = it->Next();
      if (!s.ok())
        break;
      continue;
    }

    // TODO(jsbell): Do this by direct key lookup rather than iteration, to
    // simplify.
    int64 index_id = meta_data_key.IndexId();
    base::string16 index_name;
    {
      StringPiece slice(it->Value());
      if (!DecodeString(&slice, &index_name) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
    }

    s = it->Next();  // unique flag
    if (!s.ok())
      break;
    if (!CheckIndexAndMetaDataKey(
             it.get(), stop_key, index_id, IndexMetaDataKey::UNIQUE)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
      break;
    }
    bool index_unique;
    {
      StringPiece slice(it->Value());
      if (!DecodeBool(&slice, &index_unique) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
    }

    s = it->Next();  // key_path
    if (!s.ok())
      break;
    if (!CheckIndexAndMetaDataKey(
             it.get(), stop_key, index_id, IndexMetaDataKey::KEY_PATH)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
      break;
    }
    IndexedDBKeyPath key_path;
    {
      StringPiece slice(it->Value());
      if (!DecodeIDBKeyPath(&slice, &key_path) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
    }

    s = it->Next();  // [optional] multi_entry flag
    if (!s.ok())
      break;
    bool index_multi_entry = false;
    if (CheckIndexAndMetaDataKey(
            it.get(), stop_key, index_id, IndexMetaDataKey::MULTI_ENTRY)) {
      StringPiece slice(it->Value());
      if (!DecodeBool(&slice, &index_multi_entry) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);

      s = it->Next();
      if (!s.ok())
        break;
    }

    (*indexes)[index_id] = IndexedDBIndexMetadata(
        index_name, index_id, key_path, index_unique, index_multi_entry);
  }

  if (!s.ok())
    INTERNAL_READ_ERROR_UNTESTED(GET_INDEXES);

  return s;
}

bool IndexedDBBackingStore::RemoveBlobFile(int64 database_id, int64 key) {
  FilePath fileName = GetBlobFileName(database_id, key);
  return base::DeleteFile(fileName, false);
}

bool IndexedDBBackingStore::RemoveBlobDirectory(int64 database_id) {
  FilePath dirName = GetBlobDirectoryName(blob_path_, database_id);
  return base::DeleteFile(dirName, true);
}

WARN_UNUSED_RESULT static leveldb::Status SetMaxIndexId(
    LevelDBTransaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id) {
  int64 max_index_id = -1;
  const std::string max_index_id_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::MAX_INDEX_ID);
  bool found = false;
  leveldb::Status s =
      GetInt(transaction, max_index_id_key, &max_index_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_MAX_INDEX_ID);
    return s;
  }
  if (!found)
    max_index_id = kMinimumIndexId;

  if (index_id <= max_index_id) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_MAX_INDEX_ID);
    return InternalInconsistencyStatus();
  }

  PutInt(transaction, max_index_id_key, index_id);
  return s;
}

leveldb::Status IndexedDBBackingStore::CreateIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const base::string16& name,
    const IndexedDBKeyPath& key_path,
    bool is_unique,
    bool is_multi_entry) {
  IDB_TRACE("IndexedDBBackingStore::CreateIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  leveldb::Status s = SetMaxIndexId(
      leveldb_transaction, database_id, object_store_id, index_id);

  if (!s.ok())
    return s;

  const std::string name_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::NAME);
  const std::string unique_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::UNIQUE);
  const std::string key_path_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::KEY_PATH);
  const std::string multi_entry_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::MULTI_ENTRY);

  PutString(leveldb_transaction, name_key, name);
  PutBool(leveldb_transaction, unique_key, is_unique);
  PutIDBKeyPath(leveldb_transaction, key_path_key, key_path);
  PutBool(leveldb_transaction, multi_entry_key, is_multi_entry);
  return s;
}

leveldb::Status IndexedDBBackingStore::DeleteIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id) {
  IDB_TRACE("IndexedDBBackingStore::DeleteIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string index_meta_data_start =
      IndexMetaDataKey::Encode(database_id, object_store_id, index_id, 0);
  const std::string index_meta_data_end =
      IndexMetaDataKey::EncodeMaxKey(database_id, object_store_id, index_id);
  leveldb::Status s = DeleteRange(
      leveldb_transaction, index_meta_data_start, index_meta_data_end);

  if (s.ok()) {
    const std::string index_data_start =
        IndexDataKey::EncodeMinKey(database_id, object_store_id, index_id);
    const std::string index_data_end =
        IndexDataKey::EncodeMaxKey(database_id, object_store_id, index_id);
    s = DeleteRange(leveldb_transaction, index_data_start, index_data_end);
  }

  if (!s.ok())
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_INDEX);

  return s;
}

leveldb::Status IndexedDBBackingStore::PutIndexDataForRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKey& key,
    const RecordIdentifier& record_identifier) {
  IDB_TRACE("IndexedDBBackingStore::PutIndexDataForRecord");
  DCHECK(key.IsValid());
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();

  std::string encoded_key;
  EncodeIDBKey(key, &encoded_key);

  const std::string index_data_key =
      IndexDataKey::Encode(database_id,
                           object_store_id,
                           index_id,
                           encoded_key,
                           record_identifier.primary_key(),
                           0);

  std::string data;
  EncodeVarInt(record_identifier.version(), &data);
  data.append(record_identifier.primary_key());

  transaction->transaction()->Put(index_data_key, &data);
  return leveldb::Status::OK();
}

static bool FindGreatestKeyLessThanOrEqual(LevelDBTransaction* transaction,
                                           const std::string& target,
                                           std::string* found_key,
                                           leveldb::Status& s) {
  scoped_ptr<LevelDBIterator> it = transaction->CreateIterator();
  s = it->Seek(target);
  if (!s.ok())
    return false;

  if (!it->IsValid()) {
    s = it->SeekToLast();
    if (!s.ok() || !it->IsValid())
      return false;
  }

  while (CompareIndexKeys(it->Key(), target) > 0) {
    s = it->Prev();
    if (!s.ok() || !it->IsValid())
      return false;
  }

  do {
    *found_key = it->Key().as_string();

    // There can be several index keys that compare equal. We want the last one.
    s = it->Next();
  } while (s.ok() && it->IsValid() && !CompareIndexKeys(it->Key(), target));

  return true;
}

static leveldb::Status VersionExists(LevelDBTransaction* transaction,
                                     int64 database_id,
                                     int64 object_store_id,
                                     int64 version,
                                     const std::string& encoded_primary_key,
                                     bool* exists) {
  const std::string key =
      ExistsEntryKey::Encode(database_id, object_store_id, encoded_primary_key);
  std::string data;

  leveldb::Status s = transaction->Get(key, &data, exists);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(VERSION_EXISTS);
    return s;
  }
  if (!*exists)
    return s;

  StringPiece slice(data);
  int64 decoded;
  if (!DecodeInt(&slice, &decoded) || !slice.empty())
    return InternalInconsistencyStatus();
  *exists = (decoded == version);
  return s;
}

leveldb::Status IndexedDBBackingStore::FindKeyInIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKey& key,
    std::string* found_encoded_primary_key,
    bool* found) {
  IDB_TRACE("IndexedDBBackingStore::FindKeyInIndex");
  DCHECK(KeyPrefix::ValidIds(database_id, object_store_id, index_id));

  DCHECK(found_encoded_primary_key->empty());
  *found = false;

  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  const std::string leveldb_key =
      IndexDataKey::Encode(database_id, object_store_id, index_id, key);
  scoped_ptr<LevelDBIterator> it = leveldb_transaction->CreateIterator();
  leveldb::Status s = it->Seek(leveldb_key);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(FIND_KEY_IN_INDEX);
    return s;
  }

  for (;;) {
    if (!it->IsValid())
      return leveldb::Status::OK();
    if (CompareIndexKeys(it->Key(), leveldb_key) > 0)
      return leveldb::Status::OK();

    StringPiece slice(it->Value());

    int64 version;
    if (!DecodeVarInt(&slice, &version)) {
      INTERNAL_READ_ERROR_UNTESTED(FIND_KEY_IN_INDEX);
      return InternalInconsistencyStatus();
    }
    *found_encoded_primary_key = slice.as_string();

    bool exists = false;
    s = VersionExists(leveldb_transaction,
                      database_id,
                      object_store_id,
                      version,
                      *found_encoded_primary_key,
                      &exists);
    if (!s.ok())
      return s;
    if (!exists) {
      // Delete stale index data entry and continue.
      leveldb_transaction->Remove(it->Key());
      s = it->Next();
      continue;
    }
    *found = true;
    return s;
  }
}

leveldb::Status IndexedDBBackingStore::GetPrimaryKeyViaIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKey& key,
    scoped_ptr<IndexedDBKey>* primary_key) {
  IDB_TRACE("IndexedDBBackingStore::GetPrimaryKeyViaIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();

  bool found = false;
  std::string found_encoded_primary_key;
  leveldb::Status s = FindKeyInIndex(transaction,
                                     database_id,
                                     object_store_id,
                                     index_id,
                                     key,
                                     &found_encoded_primary_key,
                                     &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_PRIMARY_KEY_VIA_INDEX);
    return s;
  }
  if (!found)
    return s;
  if (!found_encoded_primary_key.size()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_PRIMARY_KEY_VIA_INDEX);
    return InvalidDBKeyStatus();
  }

  StringPiece slice(found_encoded_primary_key);
  if (DecodeIDBKey(&slice, primary_key) && slice.empty())
    return s;
  else
    return InvalidDBKeyStatus();
}

leveldb::Status IndexedDBBackingStore::KeyExistsInIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKey& index_key,
    scoped_ptr<IndexedDBKey>* found_primary_key,
    bool* exists) {
  IDB_TRACE("IndexedDBBackingStore::KeyExistsInIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();

  *exists = false;
  std::string found_encoded_primary_key;
  leveldb::Status s = FindKeyInIndex(transaction,
                                     database_id,
                                     object_store_id,
                                     index_id,
                                     index_key,
                                     &found_encoded_primary_key,
                                     exists);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_INDEX);
    return s;
  }
  if (!*exists)
    return leveldb::Status::OK();
  if (found_encoded_primary_key.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_INDEX);
    return InvalidDBKeyStatus();
  }

  StringPiece slice(found_encoded_primary_key);
  if (DecodeIDBKey(&slice, found_primary_key) && slice.empty())
    return s;
  else
    return InvalidDBKeyStatus();
}

void IndexedDBBackingStore::ReportBlobUnused(int64 database_id,
                                             int64 blob_key) {
  // TODO(ericu)
}

IndexedDBBackingStore::Cursor::Cursor(
    const IndexedDBBackingStore::Cursor* other)
    : transaction_(other->transaction_),
      cursor_options_(other->cursor_options_),
      current_key_(new IndexedDBKey(*other->current_key_)) {
  if (other->iterator_) {
    iterator_ = transaction_->CreateIterator();

    if (other->iterator_->IsValid()) {
      leveldb::Status s = iterator_->Seek(other->iterator_->Key());
      // TODO(cmumford): Handle this error (crbug.com/363397)
      DCHECK(iterator_->IsValid());
    }
  }
}

IndexedDBBackingStore::Cursor::Cursor(LevelDBTransaction* transaction,
                                      const CursorOptions& cursor_options)
    : transaction_(transaction), cursor_options_(cursor_options) {}
IndexedDBBackingStore::Cursor::~Cursor() {}

bool IndexedDBBackingStore::Cursor::FirstSeek(leveldb::Status* s) {
  iterator_ = transaction_->CreateIterator();
  if (cursor_options_.forward)
    *s = iterator_->Seek(cursor_options_.low_key);
  else
    *s = iterator_->Seek(cursor_options_.high_key);
  if (!s->ok())
    return false;

  return Continue(0, READY, s);
}

bool IndexedDBBackingStore::Cursor::Advance(uint32 count, leveldb::Status* s) {
  *s = leveldb::Status::OK();
  while (count--) {
    if (!Continue(s))
      return false;
  }
  return true;
}

bool IndexedDBBackingStore::Cursor::Continue(const IndexedDBKey* key,
                                             const IndexedDBKey* primary_key,
                                             IteratorState next_state,
                                             leveldb::Status* s) {
  DCHECK(!key || key->IsValid());
  DCHECK(!primary_key || primary_key->IsValid());
  *s = leveldb::Status::OK();

  // TODO(alecflett): avoid a copy here?
  IndexedDBKey previous_key = current_key_ ? *current_key_ : IndexedDBKey();

  bool first_iteration = true;

  // When iterating with PrevNoDuplicate, spec requires that the
  // value we yield for each key is the first duplicate in forwards
  // order.
  IndexedDBKey last_duplicate_key;

  bool forward = cursor_options_.forward;

  for (;;) {
    if (next_state == SEEK) {
      // TODO(jsbell): Optimize seeking for reverse cursors as well.
      if (first_iteration && key && forward) {
        std::string leveldb_key;
        if (primary_key) {
          leveldb_key = EncodeKey(*key, *primary_key);
        } else {
          leveldb_key = EncodeKey(*key);
        }
        *s = iterator_->Seek(leveldb_key);
        first_iteration = false;
      } else if (forward) {
        *s = iterator_->Next();
      } else {
        *s = iterator_->Prev();
      }
      if (!s->ok())
        return false;
    } else {
      next_state = SEEK;  // for subsequent iterations
    }

    if (!iterator_->IsValid()) {
      if (!forward && last_duplicate_key.IsValid()) {
        // We need to walk forward because we hit the end of
        // the data.
        forward = true;
        continue;
      }

      return false;
    }

    if (IsPastBounds()) {
      if (!forward && last_duplicate_key.IsValid()) {
        // We need to walk forward because now we're beyond the
        // bounds defined by the cursor.
        forward = true;
        continue;
      }

      return false;
    }

    if (!HaveEnteredRange())
      continue;

    // The row may not load because there's a stale entry in the
    // index. This is not fatal.
    if (!LoadCurrentRow())
      continue;

    if (key) {
      if (forward) {
        if (primary_key && current_key_->Equals(*key) &&
            this->primary_key().IsLessThan(*primary_key))
          continue;
        if (current_key_->IsLessThan(*key))
          continue;
      } else {
        if (primary_key && key->Equals(*current_key_) &&
            primary_key->IsLessThan(this->primary_key()))
          continue;
        if (key->IsLessThan(*current_key_))
          continue;
      }
    }

    if (cursor_options_.unique) {
      if (previous_key.IsValid() && current_key_->Equals(previous_key)) {
        // We should never be able to walk forward all the way
        // to the previous key.
        DCHECK(!last_duplicate_key.IsValid());
        continue;
      }

      if (!forward) {
        if (!last_duplicate_key.IsValid()) {
          last_duplicate_key = *current_key_;
          continue;
        }

        // We need to walk forward because we hit the boundary
        // between key ranges.
        if (!last_duplicate_key.Equals(*current_key_)) {
          forward = true;
          continue;
        }

        continue;
      }
    }
    break;
  }

  DCHECK(!last_duplicate_key.IsValid() ||
         (forward && last_duplicate_key.Equals(*current_key_)));
  return true;
}

bool IndexedDBBackingStore::Cursor::HaveEnteredRange() const {
  if (cursor_options_.forward) {
    int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.low_key);
    if (cursor_options_.low_open) {
      return compare > 0;
    }
    return compare >= 0;
  }
  int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.high_key);
  if (cursor_options_.high_open) {
    return compare < 0;
  }
  return compare <= 0;
}

bool IndexedDBBackingStore::Cursor::IsPastBounds() const {
  if (cursor_options_.forward) {
    int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.high_key);
    if (cursor_options_.high_open) {
      return compare >= 0;
    }
    return compare > 0;
  }
  int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.low_key);
  if (cursor_options_.low_open) {
    return compare <= 0;
  }
  return compare < 0;
}

const IndexedDBKey& IndexedDBBackingStore::Cursor::primary_key() const {
  return *current_key_;
}

const IndexedDBBackingStore::RecordIdentifier&
IndexedDBBackingStore::Cursor::record_identifier() const {
  return record_identifier_;
}

class ObjectStoreKeyCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  ObjectStoreKeyCursorImpl(
      LevelDBTransaction* transaction,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(transaction, cursor_options) {}

  virtual Cursor* Clone() OVERRIDE {
    return new ObjectStoreKeyCursorImpl(this);
  }

  // IndexedDBBackingStore::Cursor
  virtual IndexedDBValue* value() OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual bool LoadCurrentRow() OVERRIDE;

 protected:
  virtual std::string EncodeKey(const IndexedDBKey& key) OVERRIDE {
    return ObjectStoreDataKey::Encode(
        cursor_options_.database_id, cursor_options_.object_store_id, key);
  }
  virtual std::string EncodeKey(const IndexedDBKey& key,
                                const IndexedDBKey& primary_key) OVERRIDE {
    NOTREACHED();
    return std::string();
  }

 private:
  explicit ObjectStoreKeyCursorImpl(const ObjectStoreKeyCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other) {}
};

bool ObjectStoreKeyCursorImpl::LoadCurrentRow() {
  StringPiece slice(iterator_->Key());
  ObjectStoreDataKey object_store_data_key;
  if (!ObjectStoreDataKey::Decode(&slice, &object_store_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  current_key_ = object_store_data_key.user_key();

  int64 version;
  slice = StringPiece(iterator_->Value());
  if (!DecodeVarInt(&slice, &version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  // TODO(jsbell): This re-encodes what was just decoded; try and optimize.
  std::string encoded_key;
  EncodeIDBKey(*current_key_, &encoded_key);
  record_identifier_.Reset(encoded_key, version);

  return true;
}

class ObjectStoreCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  ObjectStoreCursorImpl(
      LevelDBTransaction* transaction,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(transaction, cursor_options) {}

  virtual Cursor* Clone() OVERRIDE { return new ObjectStoreCursorImpl(this); }

  // IndexedDBBackingStore::Cursor
  virtual IndexedDBValue* value() OVERRIDE { return &current_value_; }
  virtual bool LoadCurrentRow() OVERRIDE;

 protected:
  virtual std::string EncodeKey(const IndexedDBKey& key) OVERRIDE {
    return ObjectStoreDataKey::Encode(
        cursor_options_.database_id, cursor_options_.object_store_id, key);
  }
  virtual std::string EncodeKey(const IndexedDBKey& key,
                                const IndexedDBKey& primary_key) OVERRIDE {
    NOTREACHED();
    return std::string();
  }

 private:
  explicit ObjectStoreCursorImpl(const ObjectStoreCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other),
        current_value_(other->current_value_) {}

  IndexedDBValue current_value_;
};

bool ObjectStoreCursorImpl::LoadCurrentRow() {
  StringPiece key_slice(iterator_->Key());
  ObjectStoreDataKey object_store_data_key;
  if (!ObjectStoreDataKey::Decode(&key_slice, &object_store_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  current_key_ = object_store_data_key.user_key();

  int64 version;
  StringPiece value_slice = StringPiece(iterator_->Value());
  if (!DecodeVarInt(&value_slice, &version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  // TODO(jsbell): This re-encodes what was just decoded; try and optimize.
  std::string encoded_key;
  EncodeIDBKey(*current_key_, &encoded_key);
  record_identifier_.Reset(encoded_key, version);

  current_value_.bits = value_slice.as_string();
  return true;
}

class IndexKeyCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  IndexKeyCursorImpl(
      LevelDBTransaction* transaction,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(transaction, cursor_options) {}

  virtual Cursor* Clone() OVERRIDE { return new IndexKeyCursorImpl(this); }

  // IndexedDBBackingStore::Cursor
  virtual IndexedDBValue* value() OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual const IndexedDBKey& primary_key() const OVERRIDE {
    return *primary_key_;
  }
  virtual const IndexedDBBackingStore::RecordIdentifier& record_identifier()
      const OVERRIDE {
    NOTREACHED();
    return record_identifier_;
  }
  virtual bool LoadCurrentRow() OVERRIDE;

 protected:
  virtual std::string EncodeKey(const IndexedDBKey& key) OVERRIDE {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key);
  }
  virtual std::string EncodeKey(const IndexedDBKey& key,
                                const IndexedDBKey& primary_key) OVERRIDE {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key,
                                primary_key);
  }

 private:
  explicit IndexKeyCursorImpl(const IndexKeyCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other),
        primary_key_(new IndexedDBKey(*other->primary_key_)) {}

  scoped_ptr<IndexedDBKey> primary_key_;
};

bool IndexKeyCursorImpl::LoadCurrentRow() {
  StringPiece slice(iterator_->Key());
  IndexDataKey index_data_key;
  if (!IndexDataKey::Decode(&slice, &index_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  current_key_ = index_data_key.user_key();
  DCHECK(current_key_);

  slice = StringPiece(iterator_->Value());
  int64 index_data_version;
  if (!DecodeVarInt(&slice, &index_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  if (!DecodeIDBKey(&slice, &primary_key_) || !slice.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  std::string primary_leveldb_key =
      ObjectStoreDataKey::Encode(index_data_key.DatabaseId(),
                                 index_data_key.ObjectStoreId(),
                                 *primary_key_);

  std::string result;
  bool found = false;
  leveldb::Status s = transaction_->Get(primary_leveldb_key, &result, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }
  if (!found) {
    transaction_->Remove(iterator_->Key());
    return false;
  }
  if (!result.size()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  int64 object_store_data_version;
  slice = StringPiece(result);
  if (!DecodeVarInt(&slice, &object_store_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  if (object_store_data_version != index_data_version) {
    transaction_->Remove(iterator_->Key());
    return false;
  }

  return true;
}

class IndexCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  IndexCursorImpl(
      LevelDBTransaction* transaction,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(transaction, cursor_options) {}

  virtual Cursor* Clone() OVERRIDE { return new IndexCursorImpl(this); }

  // IndexedDBBackingStore::Cursor
  virtual IndexedDBValue* value() OVERRIDE { return &current_value_; }
  virtual const IndexedDBKey& primary_key() const OVERRIDE {
    return *primary_key_;
  }
  virtual const IndexedDBBackingStore::RecordIdentifier& record_identifier()
      const OVERRIDE {
    NOTREACHED();
    return record_identifier_;
  }
  virtual bool LoadCurrentRow() OVERRIDE;

 protected:
  virtual std::string EncodeKey(const IndexedDBKey& key) OVERRIDE {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key);
  }
  virtual std::string EncodeKey(const IndexedDBKey& key,
                                const IndexedDBKey& primary_key) OVERRIDE {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key,
                                primary_key);
  }

 private:
  explicit IndexCursorImpl(const IndexCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other),
        primary_key_(new IndexedDBKey(*other->primary_key_)),
        current_value_(other->current_value_),
        primary_leveldb_key_(other->primary_leveldb_key_) {}

  scoped_ptr<IndexedDBKey> primary_key_;
  IndexedDBValue current_value_;
  std::string primary_leveldb_key_;
};

bool IndexCursorImpl::LoadCurrentRow() {
  StringPiece slice(iterator_->Key());
  IndexDataKey index_data_key;
  if (!IndexDataKey::Decode(&slice, &index_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  current_key_ = index_data_key.user_key();
  DCHECK(current_key_);

  slice = StringPiece(iterator_->Value());
  int64 index_data_version;
  if (!DecodeVarInt(&slice, &index_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }
  if (!DecodeIDBKey(&slice, &primary_key_)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  primary_leveldb_key_ =
      ObjectStoreDataKey::Encode(index_data_key.DatabaseId(),
                                 index_data_key.ObjectStoreId(),
                                 *primary_key_);

  std::string result;
  bool found = false;
  leveldb::Status s = transaction_->Get(primary_leveldb_key_, &result, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }
  if (!found) {
    transaction_->Remove(iterator_->Key());
    return false;
  }
  if (!result.size()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  int64 object_store_data_version;
  slice = StringPiece(result);
  if (!DecodeVarInt(&slice, &object_store_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  if (object_store_data_version != index_data_version) {
    transaction_->Remove(iterator_->Key());
    return false;
  }

  current_value_.bits = slice.as_string();
  return true;
}

bool ObjectStoreCursorOptions(
    LevelDBTransaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& range,
    indexed_db::CursorDirection direction,
    IndexedDBBackingStore::Cursor::CursorOptions* cursor_options) {
  cursor_options->database_id = database_id;
  cursor_options->object_store_id = object_store_id;

  bool lower_bound = range.lower().IsValid();
  bool upper_bound = range.upper().IsValid();
  cursor_options->forward =
      (direction == indexed_db::CURSOR_NEXT_NO_DUPLICATE ||
       direction == indexed_db::CURSOR_NEXT);
  cursor_options->unique = (direction == indexed_db::CURSOR_NEXT_NO_DUPLICATE ||
                            direction == indexed_db::CURSOR_PREV_NO_DUPLICATE);

  if (!lower_bound) {
    cursor_options->low_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, MinIDBKey());
    cursor_options->low_open = true;  // Not included.
  } else {
    cursor_options->low_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, range.lower());
    cursor_options->low_open = range.lowerOpen();
  }

  leveldb::Status s;

  if (!upper_bound) {
    cursor_options->high_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, MaxIDBKey());

    if (cursor_options->forward) {
      cursor_options->high_open = true;  // Not included.
    } else {
      // We need a key that exists.
      // TODO(cmumford): Handle this error (crbug.com/363397)
      if (!FindGreatestKeyLessThanOrEqual(transaction,
                                          cursor_options->high_key,
                                          &cursor_options->high_key,
                                          s))
        return false;
      cursor_options->high_open = false;
    }
  } else {
    cursor_options->high_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, range.upper());
    cursor_options->high_open = range.upperOpen();

    if (!cursor_options->forward) {
      // For reverse cursors, we need a key that exists.
      std::string found_high_key;
      // TODO(cmumford): Handle this error (crbug.com/363397)
      if (!FindGreatestKeyLessThanOrEqual(
              transaction, cursor_options->high_key, &found_high_key, s))
        return false;

      // If the target key should not be included, but we end up with a smaller
      // key, we should include that.
      if (cursor_options->high_open &&
          CompareIndexKeys(found_high_key, cursor_options->high_key) < 0)
        cursor_options->high_open = false;

      cursor_options->high_key = found_high_key;
    }
  }

  return true;
}

bool IndexCursorOptions(
    LevelDBTransaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& range,
    indexed_db::CursorDirection direction,
    IndexedDBBackingStore::Cursor::CursorOptions* cursor_options) {
  DCHECK(transaction);
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return false;

  cursor_options->database_id = database_id;
  cursor_options->object_store_id = object_store_id;
  cursor_options->index_id = index_id;

  bool lower_bound = range.lower().IsValid();
  bool upper_bound = range.upper().IsValid();
  cursor_options->forward =
      (direction == indexed_db::CURSOR_NEXT_NO_DUPLICATE ||
       direction == indexed_db::CURSOR_NEXT);
  cursor_options->unique = (direction == indexed_db::CURSOR_NEXT_NO_DUPLICATE ||
                            direction == indexed_db::CURSOR_PREV_NO_DUPLICATE);

  if (!lower_bound) {
    cursor_options->low_key =
        IndexDataKey::EncodeMinKey(database_id, object_store_id, index_id);
    cursor_options->low_open = false;  // Included.
  } else {
    cursor_options->low_key = IndexDataKey::Encode(
        database_id, object_store_id, index_id, range.lower());
    cursor_options->low_open = range.lowerOpen();
  }

  leveldb::Status s;

  if (!upper_bound) {
    cursor_options->high_key =
        IndexDataKey::EncodeMaxKey(database_id, object_store_id, index_id);
    cursor_options->high_open = false;  // Included.

    if (!cursor_options->forward) {  // We need a key that exists.
      if (!FindGreatestKeyLessThanOrEqual(transaction,
                                          cursor_options->high_key,
                                          &cursor_options->high_key,
                                          s))
        return false;
      cursor_options->high_open = false;
    }
  } else {
    cursor_options->high_key = IndexDataKey::Encode(
        database_id, object_store_id, index_id, range.upper());
    cursor_options->high_open = range.upperOpen();

    std::string found_high_key;
    // Seek to the *last* key in the set of non-unique keys
    // TODO(cmumford): Handle this error (crbug.com/363397)
    if (!FindGreatestKeyLessThanOrEqual(
            transaction, cursor_options->high_key, &found_high_key, s))
      return false;

    // If the target key should not be included, but we end up with a smaller
    // key, we should include that.
    if (cursor_options->high_open &&
        CompareIndexKeys(found_high_key, cursor_options->high_key) < 0)
      cursor_options->high_open = false;

    cursor_options->high_key = found_high_key;
  }

  return true;
}

scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& range,
    indexed_db::CursorDirection direction,
    leveldb::Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenObjectStoreCursor");
  *s = leveldb::Status::OK();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  if (!ObjectStoreCursorOptions(leveldb_transaction,
                                database_id,
                                object_store_id,
                                range,
                                direction,
                                &cursor_options))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();
  scoped_ptr<ObjectStoreCursorImpl> cursor(
      new ObjectStoreCursorImpl(leveldb_transaction, cursor_options));
  if (!cursor->FirstSeek(s))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();

  return cursor.PassAs<IndexedDBBackingStore::Cursor>();
}

scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& range,
    indexed_db::CursorDirection direction,
    leveldb::Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenObjectStoreKeyCursor");
  *s = leveldb::Status::OK();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  if (!ObjectStoreCursorOptions(leveldb_transaction,
                                database_id,
                                object_store_id,
                                range,
                                direction,
                                &cursor_options))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();
  scoped_ptr<ObjectStoreKeyCursorImpl> cursor(
      new ObjectStoreKeyCursorImpl(leveldb_transaction, cursor_options));
  if (!cursor->FirstSeek(s))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();

  return cursor.PassAs<IndexedDBBackingStore::Cursor>();
}

scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& range,
    indexed_db::CursorDirection direction,
    leveldb::Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenIndexKeyCursor");
  *s = leveldb::Status::OK();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  if (!IndexCursorOptions(leveldb_transaction,
                          database_id,
                          object_store_id,
                          index_id,
                          range,
                          direction,
                          &cursor_options))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();
  scoped_ptr<IndexKeyCursorImpl> cursor(
      new IndexKeyCursorImpl(leveldb_transaction, cursor_options));
  if (!cursor->FirstSeek(s))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();

  return cursor.PassAs<IndexedDBBackingStore::Cursor>();
}

scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& range,
    indexed_db::CursorDirection direction,
    leveldb::Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenIndexCursor");
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  if (!IndexCursorOptions(leveldb_transaction,
                          database_id,
                          object_store_id,
                          index_id,
                          range,
                          direction,
                          &cursor_options))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();
  scoped_ptr<IndexCursorImpl> cursor(
      new IndexCursorImpl(leveldb_transaction, cursor_options));
  if (!cursor->FirstSeek(s))
    return scoped_ptr<IndexedDBBackingStore::Cursor>();

  return cursor.PassAs<IndexedDBBackingStore::Cursor>();
}

IndexedDBBackingStore::Transaction::Transaction(
    IndexedDBBackingStore* backing_store)
    : backing_store_(backing_store), database_id_(-1) {}

IndexedDBBackingStore::Transaction::~Transaction() {
  STLDeleteContainerPairSecondPointers(
      blob_change_map_.begin(), blob_change_map_.end());
}

void IndexedDBBackingStore::Transaction::Begin() {
  IDB_TRACE("IndexedDBBackingStore::Transaction::Begin");
  DCHECK(!transaction_.get());
  transaction_ = new LevelDBTransaction(backing_store_->db_.get());
}

leveldb::Status IndexedDBBackingStore::Transaction::Commit() {
  IDB_TRACE("IndexedDBBackingStore::Transaction::Commit");
  DCHECK(transaction_.get());
  leveldb::Status s = transaction_->Commit();
  transaction_ = NULL;
  if (!s.ok())
    INTERNAL_WRITE_ERROR_UNTESTED(TRANSACTION_COMMIT_METHOD);
  return s;
}


class IndexedDBBackingStore::Transaction::BlobWriteCallbackWrapper
    : public IndexedDBBackingStore::BlobWriteCallback {
 public:
  BlobWriteCallbackWrapper(IndexedDBBackingStore::Transaction* transaction,
                           scoped_refptr<BlobWriteCallback> callback)
      : transaction_(transaction), callback_(callback) {}
  virtual void Run(bool succeeded) OVERRIDE {
    callback_->Run(succeeded);
    transaction_->chained_blob_writer_ = NULL;
  }

 private:
  virtual ~BlobWriteCallbackWrapper() {}
  friend class base::RefCounted<IndexedDBBackingStore::BlobWriteCallback>;

  IndexedDBBackingStore::Transaction* transaction_;
  scoped_refptr<BlobWriteCallback> callback_;
};

void IndexedDBBackingStore::Transaction::WriteNewBlobs(
    BlobEntryKeyValuePairVec& new_blob_entries,
    WriteDescriptorVec& new_files_to_write,
    scoped_refptr<BlobWriteCallback> callback) {
  DCHECK_GT(new_files_to_write.size(), 0UL);
  DCHECK_GT(database_id_, 0);
  BlobEntryKeyValuePairVec::iterator blob_entry_iter;
  for (blob_entry_iter = new_blob_entries.begin();
       blob_entry_iter != new_blob_entries.end();
       ++blob_entry_iter) {
    // Add the new blob-table entry for each blob to the main transaction, or
    // remove any entry that may exist if there's no new one.
    if (!blob_entry_iter->second.size())
      transaction_->Remove(blob_entry_iter->first.Encode());
    else
      transaction_->Put(blob_entry_iter->first.Encode(),
                        &blob_entry_iter->second);
  }
  // Creating the writer will start it going asynchronously.
  chained_blob_writer_ =
      new ChainedBlobWriterImpl(database_id_,
                                backing_store_,
                                new_files_to_write,
                                new BlobWriteCallbackWrapper(this, callback));
}

void IndexedDBBackingStore::Transaction::Rollback() {
  IDB_TRACE("IndexedDBBackingStore::Transaction::Rollback");
  DCHECK(transaction_.get());
  if (chained_blob_writer_) {
    chained_blob_writer_->Abort();
    chained_blob_writer_ = NULL;
  }
  transaction_->Rollback();
  transaction_ = NULL;
}

IndexedDBBackingStore::Transaction::BlobChangeRecord::BlobChangeRecord(
    const std::string& key, int64 object_store_id)
    : key_(key), object_store_id_(object_store_id) {
}

IndexedDBBackingStore::Transaction::BlobChangeRecord::~BlobChangeRecord() {
}

void IndexedDBBackingStore::Transaction::BlobChangeRecord::SetBlobInfo(
    std::vector<IndexedDBBlobInfo>* blob_info) {
  blob_info_.clear();
  if (blob_info)
    blob_info_.swap(*blob_info);
}

void IndexedDBBackingStore::Transaction::BlobChangeRecord::SetHandles(
    ScopedVector<webkit_blob::BlobDataHandle>* handles) {
  handles_.clear();
  if (handles)
    handles_.swap(*handles);
}

// This is storing an info, even if empty, even if the previous key had no blob
// info that we know of.  It duplicates a bunch of information stored in the
// leveldb transaction, but only w.r.t. the user keys altered--we don't keep the
// changes to exists or index keys here.
void IndexedDBBackingStore::Transaction::PutBlobInfo(
    int64 database_id,
    int64 object_store_id,
    const std::string& key,
    std::vector<IndexedDBBlobInfo>* blob_info,
    ScopedVector<webkit_blob::BlobDataHandle>* handles) {
  DCHECK_GT(key.size(), 0UL);
  if (database_id_ < 0)
    database_id_ = database_id;
  DCHECK_EQ(database_id_, database_id);

  BlobChangeMap::iterator it = blob_change_map_.find(key);
  BlobChangeRecord* record = NULL;
  if (it == blob_change_map_.end()) {
    record = new BlobChangeRecord(key, object_store_id);
    blob_change_map_[key] = record;
  } else {
    record = it->second;
  }
  DCHECK_EQ(record->object_store_id(), object_store_id);
  record->SetBlobInfo(blob_info);
  record->SetHandles(handles);
  DCHECK(!handles || !handles->size());
}

IndexedDBBackingStore::Transaction::WriteDescriptor::WriteDescriptor(
    const GURL& url,
    int64_t key)
    : is_file_(false), url_(url), key_(key) {}

IndexedDBBackingStore::Transaction::WriteDescriptor::WriteDescriptor(
    const FilePath& file_path,
    int64_t key)
    : is_file_(true), file_path_(file_path), key_(key) {}

}  // namespace content
