// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_POLICY_HANDLER_H_
#define COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace base {
class ListValue;
}

namespace bookmarks {

// Handles the ManagedBookmarks policy.
class ManagedBookmarksPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ManagedBookmarksPolicyHandler(policy::Schema chrome_schema);
  ~ManagedBookmarksPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::string GetFolderName(const base::ListValue& list);
  void FilterBookmarks(base::ListValue* bookmarks);

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarksPolicyHandler);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_POLICY_HANDLER_H_
