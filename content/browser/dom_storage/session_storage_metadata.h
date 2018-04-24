// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_METADATA_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Holds the metadata information for a session storage database. This includes
// logic for parsing and saving database content.
class CONTENT_EXPORT SessionStorageMetadata {
 public:
  static constexpr const int64_t kInvalidDatabaseVersion = -1;
  static constexpr const int64_t kInvalidMapId = -1;

  static constexpr const uint8_t kDatabaseVersionBytes[] = {'v', 'e', 'r', 's',
                                                            'i', 'o', 'n'};

  static constexpr const uint8_t kNamespacePrefixBytes[] = {
      'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e', '-'};

  // This is "next-map-id" (without the quotes).
  static constexpr const uint8_t kNextMapIdKeyBytes[] = {
      'n', 'e', 'x', 't', '-', 'm', 'a', 'p', '-', 'i', 'd'};

  // Represents a map which can be shared by multiple areas.
  // The |DeleteNamespace| and |DeleteArea| methods can destroy any MapData
  // objects who are no longer referenced by another namespace.
  class CONTENT_EXPORT MapData : public base::RefCounted<MapData> {
   public:
    explicit MapData(int64_t map_number);

    // The number of namespaces that reference this map.
    int ReferenceCount() const { return reference_count_; }

    // The key prefix for the map data (e.g. "map-2-").
    const std::vector<uint8_t>& KeyPrefix() const { return key_prefix_; }

    // The number of the map as bytes (e.g. "2").
    const std::vector<uint8_t>& MapNumberAsBytes() const {
      return number_as_bytes_;
    }

   private:
    friend class base::RefCounted<MapData>;
    friend class SessionStorageMetadata;
    ~MapData();

    void IncReferenceCount() { ++reference_count_; }
    void DecReferenceCount() { --reference_count_; }

    // The map number as bytes (e.g. "2"). These bytes are the string
    // representation of the map number.
    std::vector<uint8_t> number_as_bytes_;
    std::vector<uint8_t> key_prefix_;
    int reference_count_ = 0;
  };

  using NamespaceOriginMap =
      std::map<std::string, std::map<url::Origin, scoped_refptr<MapData>>>;
  using NamespaceEntry = NamespaceOriginMap::iterator;

  SessionStorageMetadata();
  ~SessionStorageMetadata();

  // For a new database, this sets the database version, clears the metadata,
  // and returns the operations to save to disk.
  std::vector<leveldb::mojom::BatchedOperationPtr> SetupNewDatabase(
      int64_t version);

  // This parses the database version from the bytes that were stored on
  // disk.
  bool ParseDatabaseVersion(const std::vector<uint8_t>& value);
  // Parses all namespaces and maps, and stores all metadata locally.
  // This invalidates all NamespaceEntry and MapData objects.
  // If there is a parsing error, the namespaces will be cleared.
  bool ParseNamespaces(std::vector<leveldb::mojom::KeyValuePtr> values);

  // Parses the next map id from the given bytes. If that fails, then it uses
  // the next available id from parsing the namespaces. This call is not
  // necessary on new databases.
  void ParseNextMapId(const std::vector<uint8_t>& map_id);

  int64_t NextMapId() const { return next_map_id_; }
  int64_t DatabaseVersion() const { return database_version_; }
  std::vector<uint8_t> DatabaseVersionAsVector() const;

  // Creates new map data for the given namespace-origin area. If the area
  // entry exists, then it will decrement the refcount of the old map. The
  // |save_operations| save the new or modified area entry, as well as saving
  // the next available map id.
  // Note: It is invalid to call this method for an area that has a map with
  // only one reference.
  scoped_refptr<MapData> RegisterNewMap(
      NamespaceEntry namespace_entry,
      const url::Origin& origin,
      std::vector<leveldb::mojom::BatchedOperationPtr>* save_operations);

  // Registers an origin-map in the |destination_namespace| from every
  // origin-map in the |source_namespace|. The |destination_namespace| must have
  // no origin-maps. All maps in the destination namespace are the same maps as
  // the source namespace. All database operations to save the namespace origin
  // metadata are put in |save_operations|.
  void RegisterShallowClonedNamespace(
      NamespaceEntry source_namespace,
      NamespaceEntry destination_namespace,
      std::vector<leveldb::mojom::BatchedOperationPtr>* save_operations);

  // Deletes the given namespace any any maps that no longer have any
  // references. This will invalidate all NamespaceEntry objects for the
  // |namespace_id|, and can invalidate any MapData objects whose reference
  // count hits zero.
  void DeleteNamespace(
      const std::string& namespace_id,
      std::vector<leveldb::mojom::BatchedOperationPtr>* delete_operations);

  // This removes the metadata entry for this namespace-origin area. If the map
  // at this entry isn't reference by any other area (refcount hits 0), then
  // this will delete that map on disk and invalidate that MapData.
  void DeleteArea(
      const std::string& namespace_id,
      const url::Origin& origin,
      std::vector<leveldb::mojom::BatchedOperationPtr>* delete_operations);

  NamespaceEntry GetOrCreateNamespaceEntry(const std::string& namespace_id);

  const NamespaceOriginMap& namespace_origin_map() const {
    return namespace_origin_map_;
  }

 private:
  static std::vector<uint8_t> GetNamespacePrefix(
      const std::string& namespace_id);
  static std::vector<uint8_t> GetAreaKey(const std::string& namespace_id,
                                         const url::Origin& origin);
  static std::vector<uint8_t> GetMapPrefix(int64_t map_number);
  static std::vector<uint8_t> GetMapPrefix(
      const std::vector<uint8_t>& map_number_as_bytes);

  int64_t database_version_ = kInvalidDatabaseVersion;
  int64_t next_map_id_ = kInvalidMapId;
  int64_t next_map_id_from_namespaces_ = 0;

  NamespaceOriginMap namespace_origin_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
