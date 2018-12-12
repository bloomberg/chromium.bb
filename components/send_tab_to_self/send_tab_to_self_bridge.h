// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class ModelTypeChangeProcessor;
}

namespace send_tab_to_self {

// Interface for a persistence layer for send tab to self.
// All interface methods have to be called on main thread.
class SendTabToSelfBridge : public syncer::ModelTypeSyncBridge,
                            public SendTabToSelfModel {
 public:
  explicit SendTabToSelfBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~SendTabToSelfBridge() override;

  // syncer::ModelTypeSyncBridge overrides.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // SendTabToSelfModel overrides.
  bool loaded() const override;
  const std::vector<GURL> Keys() const override;
  bool DeleteAllEntries() override;
  const SendTabToSelfEntry* GetEntryByURL(const GURL& gurl) const override;
  const SendTabToSelfEntry* GetMostRecentEntry() const override;
  const SendTabToSelfEntry* AddEntry(const GURL& url,
                                     const std::string& title) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBridge);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_
