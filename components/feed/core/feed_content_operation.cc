// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_operation.h"

#include <utility>

#include "base/logging.h"

namespace feed {

ContentOperation::ContentOperation(const ContentOperation& operation)
    : type_(operation.type_),
      key_(operation.key_),
      value_(operation.value_),
      prefix_(operation.prefix_) {}

ContentOperation::ContentOperation(ContentOperation&& operation)
    : type_(operation.type_),
      key_(std::move(operation.key_)),
      value_(std::move(operation.value_)),
      prefix_(std::move(operation.prefix_)) {}

ContentOperation ContentOperation::CreateDeleteOperation(std::string key) {
  return ContentOperation(CONTENT_DELETE, key, std::string(), std::string());
}

ContentOperation ContentOperation::CreateDeleteAllOperation() {
  return ContentOperation(CONTENT_DELETE_ALL, std::string(), std::string(),
                          std::string());
}

ContentOperation ContentOperation::CreateDeleteByPrefixOperation(
    std::string prefix) {
  return ContentOperation(CONTENT_DELETE_BY_PREFIX, std::string(),
                          std::string(), prefix);
}

ContentOperation ContentOperation::CreateUpsertOperation(std::string key,
                                                         std::string value) {
  return ContentOperation(CONTENT_UPSERT, key, value, std::string());
}

ContentOperation::Type ContentOperation::type() {
  return type_;
}

const std::string& ContentOperation::key() {
  DCHECK(type_ == CONTENT_UPSERT || type_ == CONTENT_DELETE);
  return key_;
}

const std::string& ContentOperation::value() {
  DCHECK_EQ(type_, CONTENT_UPSERT);
  return value_;
}

const std::string& ContentOperation::prefix() {
  DCHECK_EQ(type_, CONTENT_DELETE_BY_PREFIX);
  return prefix_;
}

ContentOperation::ContentOperation(Type type,
                                   std::string key,
                                   std::string value,
                                   std::string prefix)
    : type_(type), key_(key), value_(value), prefix_(prefix) {}

}  // namespace feed
