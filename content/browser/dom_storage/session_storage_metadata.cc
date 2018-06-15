// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_metadata.h"

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "content/common/dom_storage/dom_storage_namespace_ids.h"
#include "url/gurl.h"

namespace content {
namespace {
using leveldb::mojom::BatchedOperation;
using leveldb::mojom::BatchOperationType;
using leveldb::mojom::BatchedOperationPtr;

// Example layout of the database:
// | key                                    | value              |
// |----------------------------------------|--------------------|
// | map-1-a                                | b (a = b in map 1) |
// | ...                                    |                    |
// | namespace-<36 char guid 1>-origin1     | 1 (mapid)          |
// | namespace-<36 char guid 1>-origin2     | 2                  |
// | namespace-<36 char guid 2>-origin1     | 1 (shallow copy)   |
// | namespace-<36 char guid 2>-origin2     | 2 (shallow copy)   |
// | namespace-<36 char guid 3>-origin1     | 3 (deep copy)      |
// | namespace-<36 char guid 3>-origin2     | 2 (shallow copy)   |
// | next-map-id                            | 4                  |
// | version                                | 1                  |
// Example area key: namespace-dabc53e1_8291_4de5_824f_dab8aa69c846-origin2
//
// All number values (map numbers and the version) are string conversions of
// numbers. Map keys are converted to UTF-8 and the values stay as UTF-16.

// This is "map-" (without the quotes).
constexpr const uint8_t kMapIdPrefixBytes[] = {'m', 'a', 'p', '-'};

constexpr const size_t kNamespacePrefixLength =
    arraysize(SessionStorageMetadata::kNamespacePrefixBytes);
constexpr const uint8_t kNamespaceOriginSeperatorByte = '-';
constexpr const size_t kNamespaceOriginSeperatorLength = 1;
constexpr const size_t kPrefixBeforeOriginLength =
    kNamespacePrefixLength + kSessionStorageNamespaceIdLength +
    kNamespaceOriginSeperatorLength;

bool ValueToNumber(const std::vector<uint8_t>& value, int64_t* out) {
  return base::StringToInt64(leveldb::Uint8VectorToStringPiece(value), out);
}

std::vector<uint8_t> NumberToValue(int64_t map_number) {
  return leveldb::StdStringToUint8Vector(base::NumberToString(map_number));
}
}  // namespace

constexpr const int64_t SessionStorageMetadata::kMinSessionStorageSchemaVersion;
constexpr const int64_t
    SessionStorageMetadata::kLatestSessionStorageSchemaVersion;
constexpr const int64_t SessionStorageMetadata::kInvalidDatabaseVersion;
constexpr const int64_t SessionStorageMetadata::kInvalidMapId;
constexpr const uint8_t SessionStorageMetadata::kDatabaseVersionBytes[];
constexpr const uint8_t SessionStorageMetadata::kNamespacePrefixBytes[];
constexpr const uint8_t SessionStorageMetadata::kNextMapIdKeyBytes[];

SessionStorageMetadata::MapData::MapData(int64_t map_number, url::Origin origin)
    : number_as_bytes_(NumberToValue(map_number)),
      key_prefix_(SessionStorageMetadata::GetMapPrefix(number_as_bytes_)),
      origin_(std::move(origin)) {}
SessionStorageMetadata::MapData::~MapData() = default;

SessionStorageMetadata::SessionStorageMetadata() {}

SessionStorageMetadata::~SessionStorageMetadata() {}

std::vector<leveldb::mojom::BatchedOperationPtr>
SessionStorageMetadata::SetupNewDatabase() {
  next_map_id_ = 0;
  next_map_id_from_namespaces_ = 0;
  namespace_origin_map_.clear();

  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  operations.reserve(2);
  operations.push_back(BatchedOperation::New(
      BatchOperationType::PUT_KEY,
      std::vector<uint8_t>(std::begin(kDatabaseVersionBytes),
                           std::end(kDatabaseVersionBytes)),
      LatestDatabaseVersionAsVector()));
  operations.push_back(
      BatchedOperation::New(BatchOperationType::PUT_KEY,
                            std::vector<uint8_t>(std::begin(kNextMapIdKeyBytes),
                                                 std::end(kNextMapIdKeyBytes)),
                            NumberToValue(next_map_id_)));
  return operations;
}

bool SessionStorageMetadata::ParseDatabaseVersion(
    base::Optional<std::vector<uint8_t>> value,
    std::vector<leveldb::mojom::BatchedOperationPtr>* upgrade_operations) {
  if (!value) {
    initial_database_version_from_disk_ = 0;
  } else {
    if (!ValueToNumber(value.value(), &initial_database_version_from_disk_)) {
      initial_database_version_from_disk_ = kInvalidDatabaseVersion;
      return false;
    }
    if (initial_database_version_from_disk_ ==
        kLatestSessionStorageSchemaVersion)
      return true;
  }
  if (initial_database_version_from_disk_ < kMinSessionStorageSchemaVersion)
    return false;
  upgrade_operations->push_back(BatchedOperation::New(
      BatchOperationType::PUT_KEY,
      std::vector<uint8_t>(std::begin(kDatabaseVersionBytes),
                           std::end(kDatabaseVersionBytes)),
      LatestDatabaseVersionAsVector()));
  return true;
}

bool SessionStorageMetadata::ParseNamespaces(
    std::vector<leveldb::mojom::KeyValuePtr> values,
    std::vector<leveldb::mojom::BatchedOperationPtr>* upgrade_operations) {
  namespace_origin_map_.clear();
  next_map_id_from_namespaces_ = 0;
  // Since the data is ordered, all namespace data is in one spot. This keeps a
  // reference to the last namespace data map to be more efficient.
  std::string last_namespace_id;
  std::map<url::Origin, scoped_refptr<MapData>>* last_namespace = nullptr;
  std::map<int64_t, scoped_refptr<MapData>> maps;
  bool error = false;
  for (const leveldb::mojom::KeyValuePtr& key_value : values) {
    size_t key_size = key_value->key.size();

    base::StringPiece key_as_string =
        leveldb::Uint8VectorToStringPiece(key_value->key);

    if (key_size < kNamespacePrefixLength) {
      LOG(ERROR) << "Key size is less than prefix length: " << key_as_string;
      error = true;
      break;
    }

    // The key must start with 'namespace-'.
    if (!key_as_string.starts_with(base::StringPiece(
            reinterpret_cast<const char*>(kNamespacePrefixBytes),
            kNamespacePrefixLength))) {
      LOG(ERROR) << "Key must start with 'namespace-': " << key_as_string;
      error = true;
      break;
    }

    // Old databases have a dummy 'namespace-' entry.
    if (key_size == kNamespacePrefixLength)
      continue;

    // Check that the prefix is 'namespace-<guid>-
    if (key_size < kPrefixBeforeOriginLength ||
        key_as_string[kPrefixBeforeOriginLength - 1] !=
            static_cast<const char>(kNamespaceOriginSeperatorByte)) {
      LOG(ERROR) << "Prefix is not 'namespace-<guid>-': " << key_as_string;
      error = true;
      break;
    }

    // Old databases have a dummy 'namespace-<guid>-' entry.
    if (key_size == kPrefixBeforeOriginLength)
      continue;

    base::StringPiece namespace_id = key_as_string.substr(
        kNamespacePrefixLength, kSessionStorageNamespaceIdLength);

    base::StringPiece origin_str =
        key_as_string.substr(kPrefixBeforeOriginLength);

    int64_t map_number;
    if (!ValueToNumber(key_value->value, &map_number)) {
      error = true;
      LOG(ERROR) << "Could not parse map number "
                 << leveldb::Uint8VectorToStringPiece(key_value->value);
      break;
    }

    if (map_number >= next_map_id_from_namespaces_)
      next_map_id_from_namespaces_ = map_number + 1;

    auto origin_gurl = GURL(origin_str);
    if (!origin_gurl.is_valid()) {
      LOG(ERROR) << "Invalid origin " << origin_str;
      error = true;
      break;
    }

    auto origin = url::Origin::Create(origin_gurl);
    if (namespace_id != last_namespace_id) {
      last_namespace_id = namespace_id.as_string();
      DCHECK(namespace_origin_map_.find(last_namespace_id) ==
             namespace_origin_map_.end());
      last_namespace = &(namespace_origin_map_[last_namespace_id]);
    }
    auto map_it = maps.find(map_number);
    if (map_it == maps.end()) {
      map_it =
          maps.emplace(std::piecewise_construct,
                       std::forward_as_tuple(map_number),
                       std::forward_as_tuple(new MapData(map_number, origin)))
              .first;
    }
    map_it->second->IncReferenceCount();

    last_namespace->emplace(std::make_pair(std::move(origin), map_it->second));
  }
  if (error) {
    namespace_origin_map_.clear();
    next_map_id_from_namespaces_ = 0;
    return false;
  }
  if (next_map_id_ == 0 || next_map_id_ < next_map_id_from_namespaces_)
    next_map_id_ = next_map_id_from_namespaces_;

  // Namespace metadata migration.
  DCHECK_NE(kInvalidDatabaseVersion, initial_database_version_from_disk_);
  if (initial_database_version_from_disk_ == 0) {
    // Remove the dummy 'namespaces-' entry.
    upgrade_operations->push_back(BatchedOperation::New(
        BatchOperationType::DELETE_KEY,
        std::vector<uint8_t>(std::begin(kNamespacePrefixBytes),
                             std::end(kNamespacePrefixBytes)),
        base::nullopt));
    // Remove all the refcount storage.
    for (const auto& map_pair : maps) {
      upgrade_operations->push_back(
          BatchedOperation::New(BatchOperationType::DELETE_KEY,
                                map_pair.second->KeyPrefix(), base::nullopt));
    }
  }

  return true;
}

void SessionStorageMetadata::ParseNextMapId(
    const std::vector<uint8_t>& map_id) {
  if (!ValueToNumber(map_id, &next_map_id_))
    next_map_id_ = next_map_id_from_namespaces_;
  if (next_map_id_ < next_map_id_from_namespaces_)
    next_map_id_ = next_map_id_from_namespaces_;
}

// static
std::vector<uint8_t> SessionStorageMetadata::LatestDatabaseVersionAsVector() {
  return NumberToValue(kLatestSessionStorageSchemaVersion);
}

scoped_refptr<SessionStorageMetadata::MapData>
SessionStorageMetadata::RegisterNewMap(
    NamespaceEntry namespace_entry,
    const url::Origin& origin,
    std::vector<leveldb::mojom::BatchedOperationPtr>* save_operations) {
  auto new_map_data = base::MakeRefCounted<MapData>(next_map_id_, origin);
  ++next_map_id_;

  save_operations->push_back(BatchedOperation::New(
      BatchOperationType::PUT_KEY,
      std::vector<uint8_t>(
          SessionStorageMetadata::kNextMapIdKeyBytes,
          std::end(SessionStorageMetadata::kNextMapIdKeyBytes)),
      NumberToValue(next_map_id_)));

  std::map<url::Origin, scoped_refptr<MapData>>& namespace_origins =
      namespace_entry->second;
  auto namespace_it = namespace_origins.find(origin);
  if (namespace_it != namespace_origins.end()) {
    // Check the old map doesn't have the same number as the new map.
    DCHECK(namespace_it->second->MapNumberAsBytes() !=
           new_map_data->MapNumberAsBytes());
    DCHECK_GT(namespace_it->second->ReferenceCount(), 1)
        << "A new map should never be registered for an area that has a "
           "single-refcount map.";
    // There was already an area key here, so decrement that map reference.
    namespace_it->second->DecReferenceCount();
    namespace_it->second = new_map_data;
  } else {
    namespace_origins.emplace(std::make_pair(origin, new_map_data));
  }
  new_map_data->IncReferenceCount();

  save_operations->push_back(BatchedOperation::New(
      BatchOperationType::PUT_KEY, GetAreaKey(namespace_entry->first, origin),
      new_map_data->MapNumberAsBytes()));

  return new_map_data;
}

void SessionStorageMetadata::RegisterShallowClonedNamespace(
    NamespaceEntry source_namespace,
    NamespaceEntry destination_namespace,
    std::vector<leveldb::mojom::BatchedOperationPtr>* save_operations) {
  std::map<url::Origin, scoped_refptr<MapData>>& source_origins =
      source_namespace->second;
  std::map<url::Origin, scoped_refptr<MapData>>& destination_origins =
      destination_namespace->second;
  DCHECK_EQ(0ul, destination_origins.size())
      << "The destination already has data.";

  save_operations->reserve(save_operations->size() + source_origins.size());
  for (const auto& origin_map_pair : source_origins) {
    destination_origins.emplace(std::piecewise_construct,
                                std::forward_as_tuple(origin_map_pair.first),
                                std::forward_as_tuple(origin_map_pair.second));
    origin_map_pair.second->IncReferenceCount();

    save_operations->push_back(BatchedOperation::New(
        BatchOperationType::PUT_KEY,
        GetAreaKey(destination_namespace->first, origin_map_pair.first),
        origin_map_pair.second->MapNumberAsBytes()));
  }
}

void SessionStorageMetadata::DeleteNamespace(
    const std::string& namespace_id,
    std::vector<BatchedOperationPtr>* delete_operations) {
  auto it = namespace_origin_map_.find(namespace_id);
  if (it == namespace_origin_map_.end())
    return;

  delete_operations->push_back(
      BatchedOperation::New(BatchOperationType::DELETE_PREFIXED_KEY,
                            GetNamespacePrefix(namespace_id), base::nullopt));

  const std::map<url::Origin, scoped_refptr<MapData>>& origins = it->second;
  for (const auto& origin_map_pair : origins) {
    MapData* map_data = origin_map_pair.second.get();
    DCHECK_GT(map_data->ReferenceCount(), 0);
    map_data->DecReferenceCount();
    if (map_data->ReferenceCount() == 0) {
      delete_operations->push_back(
          BatchedOperation::New(BatchOperationType::DELETE_PREFIXED_KEY,
                                map_data->KeyPrefix(), base::nullopt));
    }
  }

  namespace_origin_map_.erase(it);
}

void SessionStorageMetadata::DeleteArea(
    const std::string& namespace_id,
    const url::Origin& origin,
    std::vector<BatchedOperationPtr>* delete_operations) {
  NamespaceEntry ns_entry = namespace_origin_map_.find(namespace_id);
  if (ns_entry == namespace_origin_map_.end())
    return;

  auto origin_map_it = ns_entry->second.find(origin);
  if (origin_map_it == ns_entry->second.end())
    return;

  MapData* map_data = origin_map_it->second.get();

  delete_operations->push_back(
      BatchedOperation::New(BatchOperationType::DELETE_KEY,
                            GetAreaKey(namespace_id, origin), base::nullopt));

  DCHECK_GT(map_data->ReferenceCount(), 0);
  map_data->DecReferenceCount();
  if (map_data->ReferenceCount() == 0) {
    delete_operations->push_back(
        BatchedOperation::New(BatchOperationType::DELETE_PREFIXED_KEY,
                              map_data->KeyPrefix(), base::nullopt));
  }
  ns_entry->second.erase(origin_map_it);
}

SessionStorageMetadata::NamespaceEntry
SessionStorageMetadata::GetOrCreateNamespaceEntry(
    const std::string& namespace_id) {
  // Note: if the entry exists, emplace will return the existing entry and NOT
  // insert a new entry.
  return namespace_origin_map_
      .emplace(std::piecewise_construct, std::forward_as_tuple(namespace_id),
               std::forward_as_tuple())
      .first;
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetNamespacePrefix(
    const std::string& namespace_id) {
  std::vector<uint8_t> namespace_prefix(
      SessionStorageMetadata::kNamespacePrefixBytes,
      std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  namespace_prefix.insert(namespace_prefix.end(), namespace_id.data(),
                          namespace_id.data() + namespace_id.size());
  namespace_prefix.push_back(kNamespaceOriginSeperatorByte);
  return namespace_prefix;
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetAreaKey(
    const std::string& namespace_id,
    const url::Origin& origin) {
  std::vector<uint8_t> area_key(
      SessionStorageMetadata::kNamespacePrefixBytes,
      std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  area_key.insert(area_key.end(), namespace_id.begin(), namespace_id.end());
  area_key.push_back(kNamespaceOriginSeperatorByte);
  std::string origin_str = origin.GetURL().spec();
  area_key.insert(area_key.end(), origin_str.data(),
                  origin_str.data() + origin_str.size());
  return area_key;
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetMapPrefix(int64_t map_number) {
  return GetMapPrefix(NumberToValue(map_number));
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetMapPrefix(
    const std::vector<uint8_t>& map_number_as_bytes) {
  std::vector<uint8_t> map_prefix(kMapIdPrefixBytes,
                                  std::end(kMapIdPrefixBytes));
  map_prefix.insert(map_prefix.end(), map_number_as_bytes.begin(),
                    map_number_as_bytes.end());
  map_prefix.push_back(kNamespaceOriginSeperatorByte);
  return map_prefix;
}

}  // namespace content
