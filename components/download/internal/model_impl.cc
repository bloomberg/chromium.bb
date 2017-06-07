// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/model_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/stats.h"

namespace download {

ModelImpl::ModelImpl(std::unique_ptr<Store> store)
    : client_(nullptr), store_(std::move(store)), weak_ptr_factory_(this) {
  DCHECK(store_);
}

ModelImpl::~ModelImpl() = default;

void ModelImpl::Initialize(Client* client) {
  DCHECK(!client_);
  client_ = client;
  DCHECK(client_);

  DCHECK(!store_->IsInitialized());
  store_->Initialize(base::Bind(&ModelImpl::OnInitializedFinished,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ModelImpl::Add(const Entry& entry) {
  DCHECK(store_->IsInitialized());
  DCHECK(entries_.find(entry.guid) == entries_.end());

  state_counts_[entry.state]++;
  entries_.emplace(entry.guid, base::MakeUnique<Entry>(entry));

  store_->Update(entry, base::BindOnce(&ModelImpl::OnAddFinished,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       entry.client, entry.guid));
}

void ModelImpl::Update(const Entry& entry) {
  DCHECK(store_->IsInitialized());
  DCHECK(entries_.find(entry.guid) != entries_.end());

  state_counts_[entries_[entry.guid]->state]--;
  state_counts_[entry.state]++;
  entries_[entry.guid] = base::MakeUnique<Entry>(entry);
  store_->Update(entry, base::BindOnce(&ModelImpl::OnUpdateFinished,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       entry.client, entry.guid));
}

void ModelImpl::Remove(const std::string& guid) {
  DCHECK(store_->IsInitialized());

  const auto& it = entries_.find(guid);
  DCHECK(it != entries_.end());

  DownloadClient client = it->second->client;
  state_counts_[it->second->state]--;
  entries_.erase(it);
  store_->Remove(guid,
                 base::BindOnce(&ModelImpl::OnRemoveFinished,
                                weak_ptr_factory_.GetWeakPtr(), client, guid));
}

Entry* ModelImpl::Get(const std::string& guid) {
  const auto& it = entries_.find(guid);
  return it == entries_.end() ? nullptr : it->second.get();
}

uint32_t ModelImpl::StateCount(Entry::State state) {
  return state_counts_[state];
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
  stats::LogModelOperationResult(stats::ModelAction::INITIALIZE, success);

  if (!success) {
    client_->OnModelReady(false);
    return;
  }

  for (const auto& entry : *entries) {
    state_counts_[entry.state]++;
    entries_.emplace(entry.guid, base::MakeUnique<Entry>(entry));
  }

  client_->OnModelReady(true);
}

void ModelImpl::OnAddFinished(DownloadClient client,
                              const std::string& guid,
                              bool success) {
  stats::LogModelOperationResult(stats::ModelAction::ADD, success);

  // Don't notify the Client if the entry was already removed.
  auto it = entries_.find(guid);
  if (it == entries_.end())
    return;

  // Remove the entry from the map if the add failed.
  if (!success) {
    state_counts_[it->second->state]--;
    entries_.erase(it);
  }

  client_->OnItemAdded(success, client, guid);
}

void ModelImpl::OnUpdateFinished(DownloadClient client,
                                 const std::string& guid,
                                 bool success) {
  stats::LogModelOperationResult(stats::ModelAction::UPDATE, success);

  // Don't notify the Client if the entry was already removed.
  if (entries_.find(guid) == entries_.end())
    return;

  client_->OnItemUpdated(success, client, guid);
}

void ModelImpl::OnRemoveFinished(DownloadClient client,
                                 const std::string& guid,
                                 bool success) {
  stats::LogModelOperationResult(stats::ModelAction::REMOVE, success);

  DCHECK(entries_.find(guid) == entries_.end());
  client_->OnItemRemoved(success, client, guid);
}

}  // namespace download
