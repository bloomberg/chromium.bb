// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_debug_info.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/model_impl/processor_entity_tracker.h"
#include "components/sync/protocol/proto_value_conversions.h"

using base::DictionaryValue;
using base::ListValue;

namespace syncer {

namespace {

SharedModelTypeProcessor* GetProcessorFromBridge(ModelTypeSyncBridge* bridge) {
  ModelTypeChangeProcessor* processor = bridge->change_processor();
  if (processor == nullptr) {
    LOG(WARNING) << "SharedModelTypeProcessor destroyed before debug info was "
                    "retrieved.";
  }
  return static_cast<SharedModelTypeProcessor*>(processor);
}

}  // namespace

// static
void ModelTypeDebugInfo::GetAllNodes(
    const base::Callback<void(const ModelType, std::unique_ptr<ListValue>)>&
        callback,
    ModelTypeSyncBridge* bridge) {
  SharedModelTypeProcessor* processor = GetProcessorFromBridge(bridge);
  if (processor) {
    bridge->GetAllData(base::Bind(&ModelTypeDebugInfo::MergeDataWithMetadata,
                                  base::Unretained(processor), callback));
  }
}

// static
void ModelTypeDebugInfo::GetStatusCounters(
    const base::Callback<void(ModelType, const StatusCounters&)>& callback,
    ModelTypeSyncBridge* bridge) {
  SharedModelTypeProcessor* processor = GetProcessorFromBridge(bridge);
  if (processor) {
    StatusCounters counters;
    counters.num_entries_and_tombstones = processor->entities_.size();
    for (const auto& kv : processor->entities_) {
      if (!kv.second->metadata().is_deleted()) {
        ++counters.num_entries;
      }
    }
    callback.Run(processor->type_, counters);
  }
}

// static
void ModelTypeDebugInfo::RecordMemoryUsageHistogram(
    ModelTypeSyncBridge* bridge) {
  SharedModelTypeProcessor* processor = GetProcessorFromBridge(bridge);
  size_t memory_usage = processor->EstimateMemoryUsage();
  SyncRecordMemoryKbHistogram(kModelTypeMemoryHistogramPrefix, processor->type_,
                              memory_usage);
}

ModelTypeDebugInfo::ModelTypeDebugInfo() {}

// static
void ModelTypeDebugInfo::MergeDataWithMetadata(
    SharedModelTypeProcessor* processor,
    const base::Callback<void(const ModelType, std::unique_ptr<ListValue>)>&
        callback,
    std::unique_ptr<DataBatch> batch) {
  std::unique_ptr<ListValue> all_nodes = base::MakeUnique<ListValue>();
  std::string type_string = ModelTypeToString(processor->type_);

  while (batch->HasNext()) {
    KeyAndData data = batch->Next();
    std::unique_ptr<DictionaryValue> node = data.second->ToDictionaryValue();
    ProcessorEntityTracker* entity =
        processor->GetEntityForStorageKey(data.first);
    // Entity could be null if there are some unapplied changes.
    if (entity != nullptr) {
      node->Set("metadata", EntityMetadataToValue(entity->metadata()));
    }
    node->SetString("modelType", type_string);
    all_nodes->Append(std::move(node));
  }

  // Create a permanent folder for this data type. Since sync server no longer
  // create root folders, and USS won't migrate root folders from directory, we
  // create root folders for each data type here.
  std::unique_ptr<DictionaryValue> rootnode =
      base::MakeUnique<DictionaryValue>();
  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses modelType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  rootnode->SetString("PARENT_ID", "r");
  rootnode->SetString("UNIQUE_SERVER_TAG", type_string);
  rootnode->SetBoolean("IS_DIR", true);
  rootnode->SetString("modelType", type_string);
  rootnode->SetString("NON_UNIQUE_NAME", type_string);
  all_nodes->Append(std::move(rootnode));

  callback.Run(processor->type_, std::move(all_nodes));
}

}  // namespace syncer
