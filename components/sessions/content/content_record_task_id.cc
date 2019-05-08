// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_record_task_id.h"

#include "content/public/browser/navigation_entry.h"

namespace sessions {
const char kTaskIdKey[] = "task_id_data";

ContextRecordTaskId::ContextRecordTaskId() {}

ContextRecordTaskId::ContextRecordTaskId(
    const ContextRecordTaskId& context_record_task_id) = default;

ContextRecordTaskId::~ContextRecordTaskId() {}

ContextRecordTaskId* ContextRecordTaskId::Get(content::NavigationEntry* entry) {
  ContextRecordTaskId* context_record_task_id =
      static_cast<ContextRecordTaskId*>(entry->GetUserData(kTaskIdKey));
  if (context_record_task_id)
    return context_record_task_id;
  auto context_record_task_id_ptr = base::WrapUnique(new ContextRecordTaskId());
  context_record_task_id = context_record_task_id_ptr.get();
  entry->SetUserData(kTaskIdKey, std::move(context_record_task_id_ptr));
  return context_record_task_id;
}

std::unique_ptr<base::SupportsUserData::Data> ContextRecordTaskId::Clone() {
  return base::WrapUnique(new ContextRecordTaskId(*this));
}

}  // namespace sessions
