// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_mutation.h"

#include <utility>

#include "components/feed/core/feed_content_operation.h"

namespace feed {

ContentMutation::ContentMutation() {}

ContentMutation::~ContentMutation() {}

void ContentMutation::AppendDeleteOperation(const std::string& key) {
  operations_list_.emplace_back(ContentOperation::CreateDeleteOperation(key));
}

void ContentMutation::AppendDeleteAllOperation() {
  operations_list_.emplace_back(ContentOperation::CreateDeleteAllOperation());
}

void ContentMutation::AppendDeleteByPrefixOperation(const std::string& prefix) {
  operations_list_.emplace_back(
      ContentOperation::CreateDeleteByPrefixOperation(prefix));
}

void ContentMutation::AppendUpsertOperation(const std::string& key,
                                            const std::string& value) {
  operations_list_.emplace_back(
      ContentOperation::CreateUpsertOperation(key, value));
}

ContentOperationList ContentMutation::TakeOperations() {
  return std::move(operations_list_);
}

}  // namespace feed
