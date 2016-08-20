// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_MANAGER_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_MANAGER_H_

#include "blimp/client/core/contents/blimp_contents_impl.h"

namespace blimp {
namespace client {

class TabControlFeature;

// BlimpContentsManager does the real work of creating BlimpContentsImpl, and
// then passes the ownership to the caller. It also owns the observers to
// monitor the life time of the contents it creates.
class BlimpContentsManager {
 public:
  explicit BlimpContentsManager(TabControlFeature* tab_control_feature);
  ~BlimpContentsManager();

  // Builds a BlimpContentsImpl and notifies the engine.
  std::unique_ptr<BlimpContentsImpl> CreateBlimpContents();

  // The caller can query the contents through its id.
  BlimpContentsImpl* GetBlimpContents(int id);

 private:
  class BlimpContentsDeletionObserver;
  friend class BlimpContentsDeletionObserver;

  // When creating the BlimpContentsImpl, an id is created for the content.
  int CreateBlimpContentsId();

  // When a BlimpContentsImpl is destroyed, its observer will pass the contents
  // pointer to the manager. The contents pointer is used to retrieve its id,
  // which in turn is used to destroy the observer entry from the observer_map_
  // and close the tab.
  void OnContentsDestroyed(BlimpContents* contents);

  // Destroy the observer entry from the observer_map_ when a BlimpContentsImpl
  // is destroyed.
  void EraseObserverFromMap(int id);
  base::WeakPtr<BlimpContentsManager> GetWeakPtr();

  // BlimpContentsManager owns the BlimpContentsDeletionObserver for the
  // contents it creates, with the content id being the key to help manage the
  // lifetime of the observers.
  std::map<int, std::unique_ptr<BlimpContentsDeletionObserver>> observer_map_;

  TabControlFeature* tab_control_feature_ = nullptr;

  base::WeakPtrFactory<BlimpContentsManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_MANAGER_H_
