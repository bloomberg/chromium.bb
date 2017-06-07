// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_MODEL_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_MODEL_IMPL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/internal/model.h"
#include "components/download/internal/store.h"
#include "components/download/public/clients.h"

namespace download {

struct Entry;

// The internal implementation of Model.
class ModelImpl : public Model {
 public:
  ModelImpl(std::unique_ptr<Store> store);
  ~ModelImpl() override;

  // Model implementation.
  void Initialize(Client* client) override;
  void Add(const Entry& entry) override;
  void Update(const Entry& entry) override;
  void Remove(const std::string& guid) override;
  Entry* Get(const std::string& guid) override;
  uint32_t StateCount(Entry::State state) override;
  EntryList PeekEntries() override;

 private:
  using OwnedEntryMap = std::map<std::string, std::unique_ptr<Entry>>;

  void OnInitializedFinished(bool success,
                             std::unique_ptr<std::vector<Entry>> entries);
  void OnAddFinished(DownloadClient client,
                     const std::string& guid,
                     bool success);
  void OnUpdateFinished(DownloadClient client,
                        const std::string& guid,
                        bool success);
  void OnRemoveFinished(DownloadClient client,
                        const std::string& guid,
                        bool success);

  // The external Model::Client reference that will receive all interesting
  // Model notifications.
  Client* client_;

  // The backing Store that is responsible for saving and loading the
  // persisted entries.
  std::unique_ptr<Store> store_;

  // A map of [guid] -> [std::unique_ptr<Entry>].  Effectively the cache of the
  // entries saved in Store.
  OwnedEntryMap entries_;

  // A map of Entry::State -> the total number of entries with that state.
  std::map<Entry::State, uint32_t> state_counts_;

  base::WeakPtrFactory<ModelImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModelImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_MODEL_IMPL_H_
