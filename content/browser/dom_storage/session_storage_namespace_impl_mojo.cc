// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "components/services/leveldb/public/cpp/util.h"
#include "content/public/browser/child_process_security_policy.h"

namespace content {

SessionStorageNamespaceImplMojo::SessionStorageNamespaceImplMojo(
    std::string namespace_id,
    SessionStorageDataMap::Listener* data_map_listener,
    SessionStorageAreaImpl::RegisterNewAreaMap register_new_map_callback,
    Delegate* delegate)
    : namespace_id_(std::move(namespace_id)),
      data_map_listener_(data_map_listener),
      register_new_map_callback_(std::move(register_new_map_callback)),
      delegate_(delegate) {}

SessionStorageNamespaceImplMojo::~SessionStorageNamespaceImplMojo() = default;

bool SessionStorageNamespaceImplMojo::HasAreaForOrigin(
    const url::Origin& origin) const {
  return origin_areas_.find(origin) != origin_areas_.end();
}

void SessionStorageNamespaceImplMojo::PopulateFromMetadata(
    leveldb::mojom::LevelDBDatabase* database,
    SessionStorageMetadata::NamespaceEntry namespace_metadata) {
  DCHECK(!IsPopulated());
  DCHECK(!waiting_on_clone_population());
  database_ = database;
  populated_ = true;
  namespace_entry_ = namespace_metadata;
  for (const auto& pair : namespace_entry_->second) {
    scoped_refptr<SessionStorageDataMap> data_map =
        delegate_->MaybeGetExistingDataMapForId(
            pair.second->MapNumberAsBytes());
    if (!data_map) {
      data_map = SessionStorageDataMap::Create(data_map_listener_, pair.second,
                                               database_);
    }
    origin_areas_[pair.first] = std::make_unique<SessionStorageAreaImpl>(
        namespace_entry_, pair.first, std::move(data_map),
        register_new_map_callback_);
  }
}

void SessionStorageNamespaceImplMojo::PopulateAsClone(
    leveldb::mojom::LevelDBDatabase* database,
    SessionStorageMetadata::NamespaceEntry namespace_metadata,
    const OriginAreas& areas_to_clone) {
  DCHECK(!IsPopulated());
  database_ = database;
  populated_ = true;
  waiting_on_clone_population_ = false;
  namespace_entry_ = namespace_metadata;
  std::transform(areas_to_clone.begin(), areas_to_clone.end(),
                 std::inserter(origin_areas_, origin_areas_.begin()),
                 [namespace_metadata](const auto& source) {
                   return std::make_pair(
                       source.first, source.second->Clone(namespace_metadata));
                 });
  if (!run_after_clone_population_.empty()) {
    for (base::OnceClosure& callback : run_after_clone_population_)
      std::move(callback).Run();
    run_after_clone_population_.clear();
  }
}

void SessionStorageNamespaceImplMojo::Reset() {
  namespace_entry_ = SessionStorageMetadata::NamespaceEntry();
  database_ = nullptr;
  waiting_on_clone_population_ = false;
  bind_waiting_on_clone_population_ = false;
  run_after_clone_population_.clear();
  populated_ = false;
  origin_areas_.clear();
  bindings_.CloseAllBindings();
}

void SessionStorageNamespaceImplMojo::Bind(
    blink::mojom::SessionStorageNamespaceRequest request,
    int process_id) {
  if (waiting_on_clone_population_) {
    bind_waiting_on_clone_population_ = true;
    run_after_clone_population_.push_back(
        base::BindOnce(&SessionStorageNamespaceImplMojo::Bind,
                       base::Unretained(this), std::move(request), process_id));
    return;
  }
  DCHECK(IsPopulated());
  bindings_.AddBinding(this, std::move(request), process_id);
  bind_waiting_on_clone_population_ = false;
}

void SessionStorageNamespaceImplMojo::PurgeUnboundAreas() {
  auto it = origin_areas_.begin();
  while (it != origin_areas_.end()) {
    if (!it->second->IsBound())
      it = origin_areas_.erase(it);
    else
      ++it;
  }
}

void SessionStorageNamespaceImplMojo::RemoveOriginData(
    const url::Origin& origin) {
  if (waiting_on_clone_population_) {
    run_after_clone_population_.push_back(
        base::BindOnce(&SessionStorageNamespaceImplMojo::RemoveOriginData,
                       base::Unretained(this), origin));
    return;
  }
  DCHECK(IsPopulated());
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end())
    return;
  // Renderer process expects |source| to always be two newline separated
  // strings.
  it->second->DeleteAll("\n", base::DoNothing());
  it->second->NotifyObserversAllDeleted();
  it->second->data_map()->storage_area()->ScheduleImmediateCommit();
}

void SessionStorageNamespaceImplMojo::OpenArea(
    const url::Origin& origin,
    blink::mojom::StorageAreaAssociatedRequest database) {
  DCHECK(IsPopulated());
  DCHECK(!bindings_.empty());
  int process_id = bindings_.dispatch_context();
  if (!ChildProcessSecurityPolicy::GetInstance()->CanAccessDataForOrigin(
          process_id, origin.GetURL())) {
    bindings_.ReportBadMessage("Access denied for sessionStorage request");
    return;
  }
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end()) {
    // The area may have been purged due to lack of bindings, so check the
    // metadata for the map.
    scoped_refptr<SessionStorageDataMap> data_map;
    auto map_data_it = namespace_entry_->second.find(origin);
    if (map_data_it != namespace_entry_->second.end()) {
      scoped_refptr<SessionStorageMetadata::MapData> map_data =
          map_data_it->second;
      data_map =
          delegate_->MaybeGetExistingDataMapForId(map_data->MapNumberAsBytes());
      if (!data_map) {
        data_map = SessionStorageDataMap::Create(data_map_listener_, map_data,
                                                 database_);
      }
    } else {
      data_map = SessionStorageDataMap::Create(
          data_map_listener_,
          register_new_map_callback_.Run(namespace_entry_, origin), database_);
    }
    it = origin_areas_
             .emplace(std::make_pair(
                 origin, std::make_unique<SessionStorageAreaImpl>(
                             namespace_entry_, origin, std::move(data_map),
                             register_new_map_callback_)))
             .first;
  }
  it->second->Bind(std::move(database));
}

void SessionStorageNamespaceImplMojo::Clone(
    const std::string& clone_to_namespace) {
  delegate_->RegisterShallowClonedNamespace(namespace_entry_,
                                            clone_to_namespace, origin_areas_);
}

void SessionStorageNamespaceImplMojo::FlushOriginForTesting(
    const url::Origin& origin) {
  if (!IsPopulated())
    return;
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end())
    return;
  it->second->data_map()->storage_area()->ScheduleImmediateCommit();
}

}  // namespace content
