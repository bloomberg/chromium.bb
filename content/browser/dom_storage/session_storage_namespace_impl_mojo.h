// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_MOJO_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_MOJO_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "content/browser/dom_storage/session_storage_data_map.h"
#include "content/browser/dom_storage/session_storage_leveldb_wrapper.h"
#include "content/browser/dom_storage/session_storage_metadata.h"
#include "content/browser/leveldb_wrapper_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "content/common/storage_partition_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "url/origin.h"

namespace content {
class LevelDBWrapperImpl;

// Implements the mojo interface SessionStorageNamespace. Stores data maps per
// origin, which are accessible using the LevelDBWrapper interface with the
// |OpenArea| call. Supports cloning (shallow cloning with copy-on-write
// behavior) from another SessionStorageNamespaceImplMojo.
//
// This class is populated & bound in the following patterns:
// 1. The namespace is new or being populated from data on disk, and
//    |PopulateFromMetadata| is called. Afterwards |Bind| can be called.
// 2. The namespace is being created as a clone from a |Clone| call on another
//    SessionStorageNamespaceImplMojo. PopulateFromMetadata is called with the
//    data from the other namespace, and then |Bind| can be called afterwards.
// 3. The namespace is being created as a clone, but the |Clone| call from the
//    source namespace hasn't been called yet. |SetWaitingForClonePopulation| is
//    called first, after which |Bind| can be called. The actually binding
//    doesn't happen until |PopulateAsClone| is finally called with the source
//    namespace data.
// Note: The reason for cases 2 and 3 is because there are two ways the Session
// Storage system knows about clones. First, it gets the |Clone| call on the
// source namespace, coming from the renderer doing the navigation, and in the
// correct order with any session storage modifications from that source
// renderer. Second, the RenderViewHostImpl of the navigated-to-frame will
// create the cloned namespace and expect to manage it's lifetime that way, and
// this can happen before the first case, as they are on different task runners.
class CONTENT_EXPORT SessionStorageNamespaceImplMojo final
    : public mojom::SessionStorageNamespace {
 public:
  using OriginAreas =
      std::map<url::Origin, std::unique_ptr<SessionStorageLevelDBWrapper>>;
  using RegisterShallowClonedNamespace = base::RepeatingCallback<void(
      SessionStorageMetadata::NamespaceEntry source_namespace,
      const std::string& destination_namespace,
      const OriginAreas& areas_to_clone)>;

  // Constructs a namespace with the given |namespace_id|, expecting to be
  // populated and bound later (see class comment).  The |database| and
  // |data_map_listener| are given to any data maps constructed for this
  // namespace. The |add_namespace_callback| is called when the |Clone| method
  // is called by mojo. The |register_new_map_callback| is given to the the
  // SessionStorageLevelDBWrapper's, used per-origin, that are bound to in
  // OpenArea.
  SessionStorageNamespaceImplMojo(
      std::string namespace_id,
      leveldb::mojom::LevelDBDatabase* database,
      SessionStorageDataMap::Listener* data_map_listener,
      RegisterShallowClonedNamespace add_namespace_callback,
      SessionStorageLevelDBWrapper::RegisterNewAreaMap
          register_new_map_callback);

  ~SessionStorageNamespaceImplMojo() override;

  void SetWaitingForClonePopulation() { waiting_on_clone_population_ = true; }

  bool waiting_on_clone_population() { return waiting_on_clone_population_; }

  // Called when this is a new namespace, or when the namespace was loaded from
  // disk. Should be called before |Bind|.
  void PopulateFromMetadata(
      SessionStorageMetadata::NamespaceEntry namespace_metadata);

  // Can either be called before |Bind|, or if the source namespace isn't
  // available yet, |SetWaitingForClonePopulation| can be called. Then |Bind|
  // will work, and hold onto the request until after this method is called.
  void PopulateAsClone(
      SessionStorageMetadata::NamespaceEntry namespace_metadata,
      const OriginAreas& areas_to_clone);

  SessionStorageMetadata::NamespaceEntry namespace_entry() {
    return namespace_entry_;
  }

  bool IsPopulated() const { return populated_; }

  // Must be preceded by a call to |PopulateFromMetadata|, |PopulateAsClone|, or
  // |SetWaitingForClonePopulation|. For the later case, |PopulateAsClone| must
  // eventually be called before the SessionStorageNamespaceRequest can be
  // bound.
  void Bind(mojom::SessionStorageNamespaceRequest request, int process_id);

  bool IsBound() const {
    return binding_.is_bound() || bind_waiting_on_clone_population_;
  }

  // Removes data for the given origin from this namespace. If there is no data
  // map for that given origin, this does nothing.
  void RemoveOriginData(const url::Origin& origin);

  // SessionStorageNamespace:
  // Connects the given database mojo request to the data map for the given
  // origin. Before connection, it checks to make sure the |process_id| given to
  // the |Bind| method can access the given origin.
  void OpenArea(const url::Origin& origin,
                mojom::LevelDBWrapperAssociatedRequest database) override;

  // Simply calls the |add_namespace_callback_| callback with this namespace's
  // data.
  void Clone(const std::string& clone_to_namespace) override;

  // Because LevelDBWrapper::GetAll is a sync call, for testing it's easier for
  // us to call that directly on the wrapper. Unfortunate, but better than
  // spinning up 3 threads.
  LevelDBWrapperImpl* GetWrapperForOriginForTesting(
      const url::Origin& origin) const;

 private:
  const std::string namespace_id_;
  SessionStorageMetadata::NamespaceEntry namespace_entry_;
  int process_id_ = ChildProcessHost::kInvalidUniqueID;
  leveldb::mojom::LevelDBDatabase* database_;

  SessionStorageDataMap::Listener* data_map_listener_;
  RegisterShallowClonedNamespace add_namespace_callback_;
  SessionStorageLevelDBWrapper::RegisterNewAreaMap register_new_map_callback_;

  bool waiting_on_clone_population_ = false;
  bool bind_waiting_on_clone_population_ = false;
  std::vector<base::OnceClosure> run_after_clone_population_;

  bool populated_ = false;
  OriginAreas origin_areas_;
  mojo::Binding<mojom::SessionStorageNamespace> binding_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_MOJO_H_
