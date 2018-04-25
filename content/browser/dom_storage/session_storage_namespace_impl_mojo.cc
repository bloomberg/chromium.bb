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
    leveldb::mojom::LevelDBDatabase* database,
    SessionStorageDataMap::Listener* data_map_listener,
    RegisterShallowClonedNamespace add_namespace_callback,
    SessionStorageLevelDBWrapper::RegisterNewAreaMap register_new_map_callback)
    : namespace_id_(std::move(namespace_id)),
      database_(database),
      data_map_listener_(data_map_listener),
      add_namespace_callback_(std::move(add_namespace_callback)),
      register_new_map_callback_(std::move(register_new_map_callback)),
      binding_(this) {}

SessionStorageNamespaceImplMojo::~SessionStorageNamespaceImplMojo() = default;

void SessionStorageNamespaceImplMojo::PopulateFromMetadata(
    SessionStorageMetadata::NamespaceEntry namespace_metadata) {
  DCHECK(!IsPopulated());
  DCHECK(!waiting_on_clone_population());
  populated_ = true;
  namespace_entry_ = namespace_metadata;
  for (const auto& pair : namespace_entry_->second) {
    origin_areas_[pair.first] = std::make_unique<SessionStorageLevelDBWrapper>(
        namespace_entry_, pair.first,
        SessionStorageDataMap::Create(data_map_listener_, pair.second.get(),
                                      database_),
        register_new_map_callback_);
  }
}

void SessionStorageNamespaceImplMojo::PopulateAsClone(
    SessionStorageMetadata::NamespaceEntry namespace_metadata,
    const OriginAreas& areas_to_clone) {
  DCHECK(!IsPopulated());
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

void SessionStorageNamespaceImplMojo::Bind(
    mojom::SessionStorageNamespaceRequest request,
    int process_id) {
  if (waiting_on_clone_population_) {
    bind_waiting_on_clone_population_ = true;
    run_after_clone_population_.push_back(
        base::BindOnce(&SessionStorageNamespaceImplMojo::Bind,
                       base::Unretained(this), std::move(request), process_id));
    return;
  }
  DCHECK(IsPopulated());
  process_id_ = process_id;
  binding_.Bind(std::move(request));
  bind_waiting_on_clone_population_ = false;
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
  it->second->data_map()->level_db_wrapper()->ScheduleImmediateCommit();
}

void SessionStorageNamespaceImplMojo::OpenArea(
    const url::Origin& origin,
    mojom::LevelDBWrapperAssociatedRequest database) {
  DCHECK(IsPopulated());
  DCHECK(binding_.is_bound());
  DCHECK_NE(process_id_, ChildProcessHost::kInvalidUniqueID);
  if (!ChildProcessSecurityPolicy::GetInstance()->CanAccessDataForOrigin(
          process_id_, origin.GetURL())) {
    binding_.ReportBadMessage("Access denied for sessionStorage request");
    return;
  }
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end()) {
    it = origin_areas_
             .emplace(std::make_pair(
                 origin, std::make_unique<SessionStorageLevelDBWrapper>(
                             namespace_entry_, origin,
                             SessionStorageDataMap::Create(
                                 data_map_listener_,
                                 register_new_map_callback_.Run(
                                     namespace_entry_, origin),
                                 database_),
                             register_new_map_callback_)))
             .first;
  }
  it->second->Bind(std::move(database));
}

void SessionStorageNamespaceImplMojo::Clone(
    const std::string& clone_to_namespace) {
  add_namespace_callback_.Run(namespace_entry_, clone_to_namespace,
                              origin_areas_);
}

LevelDBWrapperImpl*
SessionStorageNamespaceImplMojo::GetWrapperForOriginForTesting(
    const url::Origin& origin) const {
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end())
    return nullptr;
  return it->second->data_map()->level_db_wrapper();
}

}  // namespace content
