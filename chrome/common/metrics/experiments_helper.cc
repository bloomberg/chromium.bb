// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/experiments_helper.h"

#include <vector>

#include "base/memory/singleton.h"
#include "base/sha1.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/metrics/variation_ids.h"

namespace {

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return Singleton<GroupMapAccessor>::get();
  }

  GroupMapAccessor() {}
  ~GroupMapAccessor() {}

  void AssociateID(const experiments_helper::SelectedGroupId& group_identifier,
                   chrome_variations::ID id) {
    base::AutoLock scoped_lock(lock_);
    DCHECK(group_to_id_map_.find(group_identifier) == group_to_id_map_.end()) <<
        "You can associate a group with a chrome_variations::ID only once.";
    group_to_id_map_[group_identifier] = id;
  }

  chrome_variations::ID GetID(
      const experiments_helper::SelectedGroupId& group_identifier) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap::const_iterator it = group_to_id_map_.find(group_identifier);
    if (it == group_to_id_map_.end())
      return chrome_variations::kEmptyID;
    return it->second;
  }

 private:
  typedef std::map<experiments_helper::SelectedGroupId,
      chrome_variations::ID,
      experiments_helper::SelectedGroupIdCompare> GroupToIDMap;

  base::Lock lock_;
  GroupToIDMap group_to_id_map_;
};

// Creates unique identifier for the trial by hashing a name string, whether
// it's for the field trial or the group name.
uint32 HashName(const std::string& name) {
  // SHA-1 is designed to produce a uniformly random spread in its output space,
  // even for nearly-identical inputs.
  unsigned char sha1_hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(name.c_str()),
                      name.size(),
                      sha1_hash);

  COMPILE_ASSERT(sizeof(uint32) < sizeof(sha1_hash), need_more_data);
  uint32 bits;
  memcpy(&bits, sha1_hash, sizeof(bits));

  return base::ByteSwapToLE32(bits);
}

experiments_helper::SelectedGroupId MakeSelectedGroupId(
    const std::string& trial_name,
    const std::string& group_name) {
  experiments_helper::SelectedGroupId id;
  id.name = HashName(trial_name);
  id.group = HashName(group_name);
  return id;
}

// Populates |name_group_ids| based on |selected_groups|.
void GetFieldTrialSelectedGroupIdsForSelectedGroups(
    const base::FieldTrial::SelectedGroups& selected_groups,
    std::vector<experiments_helper::SelectedGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  for (base::FieldTrial::SelectedGroups::const_iterator it =
       selected_groups.begin(); it != selected_groups.end(); ++it) {
    name_group_ids->push_back(MakeSelectedGroupId(it->trial, it->group));
  }
}

}  // namespace

namespace experiments_helper {

void GetFieldTrialSelectedGroupIds(
    std::vector<SelectedGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  // A note on thread safety: Since GetFieldTrialSelectedGroups is thread
  // safe, and we operate on a separate list of that data, this function is
  // technically thread safe as well, with respect to the FieldTriaList data.
  base::FieldTrial::SelectedGroups selected_groups;
  base::FieldTrialList::GetFieldTrialSelectedGroups(&selected_groups);
  GetFieldTrialSelectedGroupIdsForSelectedGroups(selected_groups,
                                                 name_group_ids);
}

void AssociateGoogleExperimentID(const std::string& trial_name,
                                 const std::string& group_name,
                                 chrome_variations::ID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      MakeSelectedGroupId(trial_name, group_name), id);
}

chrome_variations::ID GetGoogleExperimentID(const std::string& trial_name,
                                            const std::string& group_name) {
  return GroupMapAccessor::GetInstance()->GetID(
      MakeSelectedGroupId(trial_name, group_name));
}

void SetChildProcessLoggingExperimentList() {
  std::vector<SelectedGroupId> name_group_ids;
  GetFieldTrialSelectedGroupIds(&name_group_ids);
  std::vector<string16> experiment_strings(name_group_ids.size());
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    // Wish there was a StringPrintf for string16... :-(
    experiment_strings[i] = WideToUTF16(base::StringPrintf(
        L"%x-%x", name_group_ids[i].name, name_group_ids[i].group));
  }
  child_process_logging::SetExperimentList(experiment_strings);
}

}  // namespace experiments_helper

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace testing {

void TestGetFieldTrialSelectedGroupIdsForSelectedGroups(
    const base::FieldTrial::SelectedGroups& selected_groups,
    std::vector<experiments_helper::SelectedGroupId>* name_group_ids) {
  ::GetFieldTrialSelectedGroupIdsForSelectedGroups(selected_groups,
                                                   name_group_ids);
}

uint32 TestHashName(const std::string& name) {
  return ::HashName(name);
}

}  // namespace testing
