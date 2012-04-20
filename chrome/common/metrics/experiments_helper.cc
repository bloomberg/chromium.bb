// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/experiments_helper.h"

#include <vector>

#include "base/memory/singleton.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"

namespace {

// We need to pass a Compare class to the std::map template since NameGroupId
// is a user-defined type.
struct NameGroupIdCompare {
  bool operator() (const base::FieldTrial::NameGroupId& lhs,
                   const base::FieldTrial::NameGroupId& rhs) const {
    // The group and name fields are just SHA-1 Hashes, so we just need to treat
    // them as IDs and do a less-than comparison. We test group first, since
    // name is more likely to collide.
    return lhs.group == rhs.group ? lhs.name < rhs.name :
        lhs.group < rhs.group;
  }
};

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return Singleton<GroupMapAccessor>::get();
  }

  GroupMapAccessor() {}
  ~GroupMapAccessor() {}

  void AssociateID(const base::FieldTrial::NameGroupId& group_identifier,
                   experiments_helper::GoogleExperimentID id) {
    base::AutoLock scoped_lock(lock_);
    DCHECK(group_to_id_map_.find(group_identifier) == group_to_id_map_.end()) <<
        "You can associate a group with a GoogleExperimentID only once.";
    group_to_id_map_[group_identifier] = id;
  }

  experiments_helper::GoogleExperimentID GetID(
      const base::FieldTrial::NameGroupId& group_identifier) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap::const_iterator it = group_to_id_map_.find(group_identifier);
    if (it == group_to_id_map_.end())
      return experiments_helper::kEmptyGoogleExperimentID;
    return it->second;
  }

 private:
  typedef std::map<base::FieldTrial::NameGroupId,
      experiments_helper::GoogleExperimentID, NameGroupIdCompare> GroupToIDMap;

  base::Lock lock_;
  GroupToIDMap group_to_id_map_;
};

}  // namespace

namespace experiments_helper {

const GoogleExperimentID kEmptyGoogleExperimentID = 0;

void AssociateGoogleExperimentID(
    const base::FieldTrial::NameGroupId& group_identifier,
    GoogleExperimentID id) {
  GroupMapAccessor::GetInstance()->AssociateID(group_identifier, id);
}

GoogleExperimentID GetGoogleExperimentID(
    const base::FieldTrial::NameGroupId& group_identifier) {
  return GroupMapAccessor::GetInstance()->GetID(group_identifier);
}

void SetChildProcessLoggingExperimentList() {
  std::vector<base::FieldTrial::NameGroupId> name_group_ids;
  base::FieldTrialList::GetFieldTrialNameGroupIds(&name_group_ids);
  std::vector<string16> experiment_strings(name_group_ids.size());
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    // Wish there was a StringPrintf for string16... :-(
    experiment_strings[i] = WideToUTF16(base::StringPrintf(
        L"%x-%x", name_group_ids[i].name, name_group_ids[i].group));
  }
  child_process_logging::SetExperimentList(experiment_strings);
}

}  // namespace experiments_helper
