// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/public/contents/blimp_contents_observer.h"

namespace {
const int kDummyTabId = 0;
}

namespace blimp {
namespace client {

class BlimpContentsManager::BlimpContentsDeletionObserver
    : public BlimpContentsObserver {
 public:
  BlimpContentsDeletionObserver(BlimpContentsManager* blimp_contents_manager,
                                BlimpContentsImpl* blimp_contents);

  void OnContentsDestroyed() override;

 private:
  // The BlimpContentsManager containing this BlimpContentsDeletionObserver
  BlimpContentsManager* blimp_contents_manager_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsDeletionObserver);
};

BlimpContentsManager::BlimpContentsDeletionObserver::
    BlimpContentsDeletionObserver(BlimpContentsManager* blimp_contents_manager,
                                  BlimpContentsImpl* blimp_contents)
    : BlimpContentsObserver(blimp_contents),
      blimp_contents_manager_(blimp_contents_manager) {}

void BlimpContentsManager::BlimpContentsDeletionObserver::
    OnContentsDestroyed() {
  BlimpContents* contents = blimp_contents();
  blimp_contents_manager_->OnContentsDestroyed(contents);
}

BlimpContentsManager::BlimpContentsManager(
    TabControlFeature* tab_control_feature)
    : tab_control_feature_(tab_control_feature),
      weak_ptr_factory_(this) {}

BlimpContentsManager::~BlimpContentsManager() {}

std::unique_ptr<BlimpContentsImpl> BlimpContentsManager::CreateBlimpContents() {
  int id = CreateBlimpContentsId();

  std::unique_ptr<BlimpContentsImpl> new_contents =
      base::MakeUnique<BlimpContentsImpl>(id, tab_control_feature_);

  // Create an observer entry for the contents.
  std::unique_ptr<BlimpContentsDeletionObserver> observer =
      base::MakeUnique<BlimpContentsDeletionObserver>(this, new_contents.get());
  observer_map_.insert(
      std::pair<int, std::unique_ptr<BlimpContentsDeletionObserver>>(
          id, std::move(observer)));

  // Notifies the engine that we've created a new BlimpContents.
  tab_control_feature_->CreateTab(id);

  return new_contents;
}

BlimpContentsImpl* BlimpContentsManager::GetBlimpContents(int id) {
  if (observer_map_.find(id) == observer_map_.end()) return nullptr;

  BlimpContentsDeletionObserver* observer = observer_map_.at(id).get();
  // If the BlimpContents that the observer tracks is empty, it means
  // OnContentsDestroyed was called on this observer, but the task to erase
  // the observer from the map hasn't been run.
  if (observer->blimp_contents())
    return static_cast<BlimpContentsImpl*>(observer->blimp_contents());

  return nullptr;
}

int BlimpContentsManager::CreateBlimpContentsId() {
  // TODO(mlliu): currently, Blimp only supports a single tab, so returning a
  // dummy tab id. Need to return real case id when Blimp supports multiple
  // tabs.
  return kDummyTabId;
}

void BlimpContentsManager::EraseObserverFromMap(int id) {
  observer_map_.erase(id);
}

void BlimpContentsManager::OnContentsDestroyed(BlimpContents* contents) {
  int id = static_cast<BlimpContentsImpl*>(contents)->id();

  // Notify the engine that we've destroyed the BlimpContents.
  tab_control_feature_->CloseTab(id);

  // Destroy the observer entry from the observer_map_.
  DCHECK(base::ThreadTaskRunnerHandle::Get());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BlimpContentsManager::EraseObserverFromMap,
                            this->GetWeakPtr(), id));
}

base::WeakPtr<BlimpContentsManager> BlimpContentsManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace client
}  // namespace blimp
