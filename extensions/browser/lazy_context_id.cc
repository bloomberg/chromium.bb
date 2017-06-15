// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_context_id.h"

#include "extensions/browser/lazy_background_task_queue.h"

namespace extensions {

LazyContextId::LazyContextId(content::BrowserContext* context,
                             const ExtensionId& extension_id)
    : type_(Type::kEventPage), context_(context), extension_id_(extension_id) {}

LazyBackgroundTaskQueue* LazyContextId::GetTaskQueue() {
  DCHECK(is_for_event_page());
  return LazyBackgroundTaskQueue::Get(context_);
}

}  // namespace extensions
