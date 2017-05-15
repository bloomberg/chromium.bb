// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/model_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/download/internal/entry.h"

namespace download {

ModelImpl::ModelImpl(Client* client, std::unique_ptr<Store> store)
    : client_(client), store_(std::move(store)), weak_ptr_factory_(this) {
  DCHECK(client_);
  DCHECK(store_);
}

ModelImpl::~ModelImpl() = default;

void ModelImpl::Initialize() {
  DCHECK(!store_->IsInitialized());
  store_->Initialize(base::Bind(&ModelImpl::OnInitializedFinished,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ModelImpl::Destroy() {
  store_->Destroy(base::Bind(&ModelImpl::OnDestroyFinished,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ModelImpl::Add(const Entry& entry) {
  DCHECK(store_->IsInitialized());
  DCHECK(entries_.find(entry.guid) == entries_.end());

  entries_.emplace(entry.guid, base::MakeUnique<Entry>(entry));

  store_->Update(entry, base::Bind(&ModelImpl::OnAddFinished,
                                   weak_ptr_factory_.GetWeakPtr(), entry.client,
                                   entry.guid));
}

void ModelImpl::Update(const Entry& entry) {
  DCHECK(store_->IsInitialized());
  DCHECK(entries_.find(entry.guid) != entries_.end());

  entries_[entry.guid] = base::MakeUnique<Entry>(entry);
  store_->Update(entry, base::Bind(&ModelImpl::OnUpdateFinished,
                                   weak_ptr_factory_.GetWeakPtr(), entry.client,
                                   entry.guid));
}

void ModelImpl::Remove(const std::string& guid) {
  DCHECK(store_->IsInitialized());

  const auto& it = entries_.find(guid);
  DCHECK(it != entries_.end());

  DownloadClient client = it->second->client;
  entries_.erase(it);
  store_->Remove(guid,
                 base::Bind(&ModelImpl::OnRemoveFinished,
                            weak_ptr_factory_.GetWeakPtr(), client, guid));
}

Entry* ModelImpl::Get(const std::string& guid) {
  const auto& it = entries_.find(guid);
  return it == entries_.end() ? nullptr : it->second.get();
}

Model::EntryList ModelImpl::PeekEntries() {
  EntryList entries;
  for (const auto& it : entries_)
    entries.push_back(it.second.get());

  return entries;
}

void ModelImpl::OnInitializedFinished(
    bool success,
    std::unique_ptr<std::vector<Entry>> entries) {
  if (!success) {
    client_->OnInitialized(false);
    return;
  }

  for (const auto& entry : *entries)
    entries_.emplace(entry.guid, base::MakeUnique<Entry>(entry));

  client_->OnInitialized(true);
}

void ModelImpl::OnDestroyFinished(bool success) {
  store_.reset();
  entries_.clear();
  client_->OnDestroyed(success);
}

void ModelImpl::OnAddFinished(DownloadClient client,
                              const std::string& guid,
                              bool success) {
  // Don't notify the Client if the entry was already removed.
  if (entries_.find(guid) == entries_.end())
    return;

  // Remove the entry from the map if the add failed.
  if (!success)
    entries_.erase(guid);

  client_->OnItemAdded(success, client, guid);
}

void ModelImpl::OnUpdateFinished(DownloadClient client,
                                 const std::string& guid,
                                 bool success) {
  // Don't notify the Client if the entry was already removed.
  if (entries_.find(guid) == entries_.end())
    return;

  client_->OnItemUpdated(success, client, guid);
}

void ModelImpl::OnRemoveFinished(DownloadClient client,
                                 const std::string& guid,
                                 bool success) {
  DCHECK(entries_.find(guid) == entries_.end());
  client_->OnItemRemoved(success, client, guid);
}

}  // namespace download
