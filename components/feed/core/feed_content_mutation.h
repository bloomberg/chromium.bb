// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_CONTENT_MUTATION_H_
#define COMPONENTS_FEED_CORE_FEED_CONTENT_MUTATION_H_

#include <list>
#include <string>

#include "base/macros.h"

namespace feed {

class ContentOperation;
using ContentOperationList = std::list<ContentOperation>;

// Native counterpart of ContentMutation.java.
// To commit a set of ContentOperation into FeedContentDatabase, user need to
// create a ContentMutation, next use Append* methods to append operations
// into the mutation, and then pass the ContentMutation to
// |FeedContentDatabase::CommitContentMutation| to commit operations.
class ContentMutation {
 public:
  ContentMutation();
  ~ContentMutation();

  void AppendDeleteOperation(const std::string& key);
  void AppendDeleteAllOperation();
  void AppendDeleteByPrefixOperation(const std::string& prefix);
  void AppendUpsertOperation(const std::string& key, const std::string& value);

  // This will std::move the |operations_list_| out.
  ContentOperationList TakeOperations();

 private:
  ContentOperationList operations_list_;

  DISALLOW_COPY_AND_ASSIGN(ContentMutation);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_CONTENT_MUTATION_H_
