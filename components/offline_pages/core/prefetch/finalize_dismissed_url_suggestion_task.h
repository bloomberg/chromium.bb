// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_FINALIZE_DISMISSED_URL_SUGGESTION_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_FINALIZE_DISMISSED_URL_SUGGESTION_TASK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {
struct ClientId;
class PrefetchStore;

// A Task that marks a PrefetchItem as finished with a SUGGESTION_INVALIDATED
// error code if it matches a ClientId, but only when the page has not been (at
// least partially) downloaded.
class FinalizeDismissedUrlSuggestionTask : public Task {
 public:
  FinalizeDismissedUrlSuggestionTask(PrefetchStore* prefetch_store,
                                     const ClientId& client_id);
  ~FinalizeDismissedUrlSuggestionTask() override;

  void Run() override;

 private:
  void OnComplete(bool removed);

  PrefetchStore* prefetch_store_;
  ClientId client_id_;
  base::WeakPtrFactory<FinalizeDismissedUrlSuggestionTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FinalizeDismissedUrlSuggestionTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_FINALIZE_DISMISSED_URL_SUGGESTION_TASK_H_
