// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/public/graph/node_attached_data.h"

namespace performance_manager {

class NodeBase;

// Helper class for providing internal storage of a NodeAttachedData
// implementation directly in a node. The storage is provided as a raw buffer of
// bytes which is initialized externally by the NodeAttachedDataImpl via a
// placement new. In this way the node only needs to know about the
// NodeAttachedData base class, and the size of the required storage.
template <size_t DataSize>
class InternalNodeAttachedDataStorage {
 public:
  static constexpr size_t kDataSize = DataSize;

  InternalNodeAttachedDataStorage() {}

  ~InternalNodeAttachedDataStorage() { Reset(); }

  // Returns a pointer to the data object, if allocated.
  NodeAttachedData* Get() { return data_; }

  void Reset() {
    if (data_)
      data_->~NodeAttachedData();
    data_ = nullptr;
  }

  uint8_t* buffer() { return buffer_; }

 protected:
  friend class InternalNodeAttachedDataStorageAccess;

  // Transitions this object to being allocated.
  void Set(NodeAttachedData* data) {
    DCHECK(!data_);
    // Depending on the object layout, once it has been cast to a
    // NodeAttachedData there's no guarantee that the pointer will be at the
    // head of the object, only that the pointer will be somewhere inside of the
    // full object extent.
    DCHECK_LE(buffer_, reinterpret_cast<uint8_t*>(data));
    DCHECK_GT(buffer_ + kDataSize, reinterpret_cast<uint8_t*>(data));
    data_ = data;
  }

 private:
  NodeAttachedData* data_ = nullptr;
  uint8_t buffer_[kDataSize];
  DISALLOW_COPY_AND_ASSIGN(InternalNodeAttachedDataStorage);
};

// Provides access for setting/getting data in the map based storage. Not
// intended to be used directly, but rather by (External)NodeAttachedDataImpl.
class NodeAttachedDataMapHelper {
 public:
  // Attaches the provided |data| to the provided |node|. This should only be
  // called if the data does not exist (GetFromMap returns nullptr), and will
  // DCHECK otherwise.
  static void AttachInMap(const NodeBase* node,
                          std::unique_ptr<NodeAttachedData> data);

  // Retrieves the data associated with the provided |node| and |key|. This
  // returns nullptr if no data exists.
  static NodeAttachedData* GetFromMap(const NodeBase* node, const void* key);

  // Detaches the data associated with the provided |node| and |key|. It is
  // harmless to call this when no data exists.
  static std::unique_ptr<NodeAttachedData> DetachFromMap(const NodeBase* node,
                                                         const void* key);
};

// Implementation of ExternalNodeAttachedDataImpl, which is declared in the
// corresponding public header. This helps keep the public headers as clean as
// possible.

// static
template <typename UserDataType>
constexpr int ExternalNodeAttachedDataImpl<UserDataType>::kUserDataKey;

template <typename UserDataType>
template <typename NodeType>
UserDataType* ExternalNodeAttachedDataImpl<UserDataType>::GetOrCreate(
    const NodeType* node) {
  if (auto* data = Get(node))
    return data;
  std::unique_ptr<UserDataType> data = std::make_unique<UserDataType>(node);
  auto* raw_data = data.get();
  auto* base = reinterpret_cast<const NodeBase*>(node->GetIndexingKey());
  NodeAttachedDataMapHelper::AttachInMap(base, std::move(data));
  return raw_data;
}

template <typename UserDataType>
template <typename NodeType>
UserDataType* ExternalNodeAttachedDataImpl<UserDataType>::Get(
    const NodeType* node) {
  auto* base = reinterpret_cast<const NodeBase*>(node->GetIndexingKey());
  auto* data = NodeAttachedDataMapHelper::GetFromMap(base, UserDataKey());
  if (!data)
    return nullptr;
  DCHECK_EQ(UserDataKey(), data->GetKey());
  return static_cast<UserDataType*>(data);
}

template <typename UserDataType>
template <typename NodeType>
bool ExternalNodeAttachedDataImpl<UserDataType>::Destroy(const NodeType* node) {
  auto* base = reinterpret_cast<const NodeBase*>(node->GetIndexingKey());
  std::unique_ptr<NodeAttachedData> data =
      NodeAttachedDataMapHelper::DetachFromMap(base, UserDataKey());
  return data.get();
}

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_
