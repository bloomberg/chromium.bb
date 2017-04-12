// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/android/web_app_manifest_section_table.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace payments {
namespace {

// Note that the fingerprint is calculated with SHA-256.
const size_t kFingerPrintLength = 32;

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

// Converts 2-dimensional vector |fingerprints| to 1-dimesional vector.
std::unique_ptr<std::vector<uint8_t>> SerializeFingerPrints(
    const std::vector<std::vector<uint8_t>>& fingerprints) {
  auto serialized_fingerprints = base::MakeUnique<std::vector<uint8_t>>();

  for (const auto& fingerprint : fingerprints) {
    DCHECK_EQ(fingerprint.size(), kFingerPrintLength);
    serialized_fingerprints->insert(serialized_fingerprints->end(),
                                    fingerprint.begin(), fingerprint.end());
  }

  return serialized_fingerprints;
}

// Converts 1-dimensional vector created by SerializeFingerPrints back to
// 2-dimensional vector. Each vector of the second dimensional vector has exact
// kFingerPrintLength number of elements.
bool DeserializeFingerPrints(
    const std::vector<uint8_t>& fingerprints,
    std::vector<std::vector<uint8_t>>& deserialized_fingerprints) {
  if (fingerprints.size() % kFingerPrintLength != 0)
    return false;

  for (size_t i = 0; i < fingerprints.size(); i += kFingerPrintLength) {
    deserialized_fingerprints.emplace_back(
        fingerprints.begin() + i,
        fingerprints.begin() + i + kFingerPrintLength);
  }
  return true;
}

}  // namespace

WebAppManifestSectionTable::WebAppManifestSectionTable() {}

WebAppManifestSectionTable::~WebAppManifestSectionTable() {}

WebAppManifestSectionTable* WebAppManifestSectionTable::FromWebDatabase(
    WebDatabase* db) {
  return static_cast<WebAppManifestSectionTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey WebAppManifestSectionTable::GetTypeKey() const {
  return GetKey();
}

bool WebAppManifestSectionTable::CreateTablesIfNecessary() {
  if (!db_->Execute("CREATE TABLE IF NOT EXISTS web_app_manifest_section ( "
                    "id VARCHAR PRIMARY KEY, "
                    "min_version INTEGER NOT NULL DEFAULT 0, "
                    "fingerprints BLOB) ")) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebAppManifestSectionTable::IsSyncable() {
  return false;
}

bool WebAppManifestSectionTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  return true;
}

bool WebAppManifestSectionTable::AddWebAppManifest(
    mojom::WebAppManifestSection* manifest) {
  DCHECK(manifest);
  DCHECK(!manifest->id.empty());

  sql::Statement s(
      db_->GetUniqueStatement("INSERT OR REPLACE INTO web_app_manifest_section "
                              "(id, min_version, fingerprints) "
                              "VALUES (?, ?, ?)"));
  int index = 0;
  s.BindString(index++, manifest->id);
  s.BindInt64(index++, manifest->min_version);
  std::unique_ptr<std::vector<uint8_t>> serialized_fingerprints =
      SerializeFingerPrints(manifest->fingerprints);
  s.BindBlob(index, serialized_fingerprints->data(),
             serialized_fingerprints->size());
  if (!s.Run())
    return false;

  return true;
}

mojom::WebAppManifestSectionPtr WebAppManifestSectionTable::GetWebAppManifest(
    const std::string& web_app) {
  sql::Statement s(
      db_->GetUniqueStatement("SELECT id, min_version, fingerprints "
                              "FROM web_app_manifest_section "
                              "WHERE id=?"));
  s.BindString(0, web_app);

  if (!s.Step())
    return nullptr;

  mojom::WebAppManifestSectionPtr manifest =
      mojom::WebAppManifestSection::New();

  int index = 0;
  manifest->id = s.ColumnString(index++);
  manifest->min_version = s.ColumnInt64(index++);

  std::vector<uint8_t> fingerprints;
  if (!s.ColumnBlobAsVector(index, &fingerprints))
    return nullptr;

  if (!DeserializeFingerPrints(fingerprints, manifest->fingerprints))
    return nullptr;

  return manifest;
}

}  // payments
