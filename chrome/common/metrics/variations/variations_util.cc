// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/variations/variations_util.h"

#include <map>
#include <vector>

#include "base/memory/singleton.h"
#include "base/sha1.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/metrics/metrics_util.h"
#include "chrome/common/metrics/variations/variation_ids.h"

namespace chrome_variations {

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

  // Note that this normally only sets the ID for a group the first time, unless
  // |force| is set to true, in which case it will always override it.
  void AssociateID(const SelectedGroupId& group_identifier,
                   const VariationID id,
                   const bool force) {
    base::AutoLock scoped_lock(lock_);
    if (force ||
        group_to_id_map_.find(group_identifier) == group_to_id_map_.end())
      group_to_id_map_[group_identifier] = id;
  }

  VariationID GetID(const SelectedGroupId& group_identifier) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap::const_iterator it = group_to_id_map_.find(group_identifier);
    if (it == group_to_id_map_.end())
      return kEmptyID;
    return it->second;
  }

 private:
  typedef std::map<SelectedGroupId, VariationID, SelectedGroupIdCompare>
      GroupToIDMap;

  base::Lock lock_;
  GroupToIDMap group_to_id_map_;

  DISALLOW_COPY_AND_ASSIGN(GroupMapAccessor);
};

SelectedGroupId MakeSelectedGroupId(const std::string& trial_name,
                                    const std::string& group_name) {
  SelectedGroupId id;
  id.name = metrics::HashName(trial_name);
  id.group = metrics::HashName(group_name);
  return id;
}

// Populates |name_group_ids| based on |selected_groups|.
void GetFieldTrialSelectedGroupIdsForSelectedGroups(
    const base::FieldTrial::SelectedGroups& selected_groups,
    std::vector<SelectedGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  for (base::FieldTrial::SelectedGroups::const_iterator it =
       selected_groups.begin(); it != selected_groups.end(); ++it) {
    name_group_ids->push_back(MakeSelectedGroupId(it->trial, it->group));
  }
}

}  // namespace

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

void GetFieldTrialSelectedGroupIdsAsStrings(
    std::vector<string16>* output) {
  DCHECK(output->empty());
  std::vector<SelectedGroupId> name_group_ids;
  GetFieldTrialSelectedGroupIds(&name_group_ids);
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    output->push_back(UTF8ToUTF16(base::StringPrintf(
        "%x-%x", name_group_ids[i].name, name_group_ids[i].group)));
  }
}

void AssociateGoogleVariationID(const std::string& trial_name,
                                const std::string& group_name,
                                chrome_variations::VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      MakeSelectedGroupId(trial_name, group_name), id, false);
}

void AssociateGoogleVariationIDForce(const std::string& trial_name,
                                     const std::string& group_name,
                                     chrome_variations::VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      MakeSelectedGroupId(trial_name, group_name), id, true);
}

chrome_variations::VariationID GetGoogleVariationID(
    const std::string& trial_name,
    const std::string& group_name) {
  return GroupMapAccessor::GetInstance()->GetID(
      MakeSelectedGroupId(trial_name, group_name));
}

void GenerateVariationChunks(const std::vector<string16>& experiments,
                             std::vector<string16>* chunks) {
  string16 current_chunk;
  for (size_t i = 0; i < experiments.size(); ++i) {
    const size_t needed_length =
        (current_chunk.empty() ? 1 : 0) + experiments[i].length();
    if (current_chunk.length() + needed_length > kMaxVariationChunkSize) {
      chunks->push_back(current_chunk);
      current_chunk = experiments[i];
    } else {
      if (!current_chunk.empty())
        current_chunk.push_back(',');
      current_chunk += experiments[i];
    }
  }
  if (!current_chunk.empty())
    chunks->push_back(current_chunk);
}

void SetChildProcessLoggingVariationList() {
  std::vector<string16> experiment_strings;
  GetFieldTrialSelectedGroupIdsAsStrings(&experiment_strings);
  child_process_logging::SetExperimentList(experiment_strings);
}

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace testing {

void TestGetFieldTrialSelectedGroupIdsForSelectedGroups(
    const base::FieldTrial::SelectedGroups& selected_groups,
    std::vector<SelectedGroupId>* name_group_ids) {
  GetFieldTrialSelectedGroupIdsForSelectedGroups(selected_groups,
                                                 name_group_ids);
}

}  // namespace testing

}  // namespace chrome_variations

