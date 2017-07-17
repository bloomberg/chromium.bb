// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_TEST_NOOP_STORE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_TEST_NOOP_STORE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/internal/store.h"

namespace download {

struct Entry;

// A Store implementation that doesn't do anything but honors the interface
// requirements.
// TODO(dtrainor, shaktisahu): Remove this if it's no longer necessary after the
// real Store implementation is added.
class NoopStore : public Store {
 public:
  NoopStore();
  ~NoopStore() override;

  // Store implementation.
  bool IsInitialized() override;
  void Initialize(InitCallback callback) override;
  void HardRecover(StoreCallback callback) override;
  void Update(const Entry& entry, StoreCallback callback) override;
  void Remove(const std::string& guid, StoreCallback callback) override;

 private:
  void OnInitFinished(InitCallback callback);

  // Whether or not this Store is 'initialized.'  Just gets set to |true| once
  // Initialize() is called.
  bool initialized_;

  base::WeakPtrFactory<NoopStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NoopStore);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_TEST_NOOP_STORE_H_
