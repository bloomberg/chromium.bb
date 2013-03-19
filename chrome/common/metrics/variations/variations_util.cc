// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/variations/variations_util.h"

#include <map>
#include <vector>

#include "base/memory/singleton.h"
#include "base/sha1.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_byteorder.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/metrics/metrics_util.h"
#include "chrome/common/metrics/variations/variation_ids.h"
#include "chrome/installer/util/google_update_experiment_util.h"

namespace chrome_variations {

namespace {

const char kVariationPrefix[] = "CrVar";
const char kExperimentLabelSep[] = ";";

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  typedef std::map<ActiveGroupId, VariationID, ActiveGroupIdCompare>
      GroupToIDMap;

  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return Singleton<GroupMapAccessor>::get();
  }

  // Note that this normally only sets the ID for a group the first time, unless
  // |force| is set to true, in which case it will always override it.
  void AssociateID(IDCollectionKey key,
                   const ActiveGroupId& group_identifier,
                   const VariationID id,
                   const bool force) {
#if !defined(NDEBUG)
    // Validate that all collections with this |group_identifier| have the same
    // associated ID.
    DCHECK_EQ(2, ID_COLLECTION_COUNT);
    IDCollectionKey other_key = GOOGLE_WEB_PROPERTIES;
    if (key == GOOGLE_WEB_PROPERTIES)
      other_key = GOOGLE_UPDATE_SERVICE;
    VariationID other_id = GetID(other_key, group_identifier);
    DCHECK(other_id == EMPTY_ID || other_id == id);
#endif

    base::AutoLock scoped_lock(lock_);

    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    if (force ||
        group_to_id_map->find(group_identifier) == group_to_id_map->end())
      (*group_to_id_map)[group_identifier] = id;
  }

  VariationID GetID(IDCollectionKey key,
                    const ActiveGroupId& group_identifier) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    GroupToIDMap::const_iterator it = group_to_id_map->find(group_identifier);
    if (it == group_to_id_map->end())
      return EMPTY_ID;
    return it->second;
  }

  void ClearAllMapsForTesting() {
    base::AutoLock scoped_lock(lock_);

    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      GroupToIDMap* map = GetGroupToIDMap(static_cast<IDCollectionKey>(i));
      DCHECK(map);
      map->clear();
    }
  }

 private:
  friend struct DefaultSingletonTraits<GroupMapAccessor>;

  // Retrieves the GroupToIDMap for |key|.
  GroupToIDMap* GetGroupToIDMap(IDCollectionKey key) {
    return &group_to_id_maps_[key];
  }

  GroupMapAccessor() {
    group_to_id_maps_.resize(ID_COLLECTION_COUNT);
  }
  ~GroupMapAccessor() {}

  base::Lock lock_;
  std::vector<GroupToIDMap> group_to_id_maps_;

  DISALLOW_COPY_AND_ASSIGN(GroupMapAccessor);
};

ActiveGroupId MakeActiveGroupId(const std::string& trial_name,
                                const std::string& group_name) {
  ActiveGroupId id;
  id.name = metrics::HashName(trial_name);
  id.group = metrics::HashName(group_name);
  return id;
}

// Populates |name_group_ids| based on |active_groups|.
void GetFieldTrialActiveGroupIdsForActiveGroups(
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    name_group_ids->push_back(MakeActiveGroupId(it->trial_name,
                                                it->group_name));
  }
}

// This method builds a single experiment label for a Chrome Variation,
// including a timestamp that is a year in the future from now. Since multiple
// headers can be transmitted, |count| is a number that is appended after the
// label key to differentiate the labels.
string16 CreateSingleExperimentLabel(int count,
                                     chrome_variations::VariationID id) {
  // Build the parts separately so they can be validated.
  const string16 key =
      ASCIIToUTF16(kVariationPrefix) + base::IntToString16(count);
  DCHECK_LE(key.size(), 8U);
  const string16 value = base::IntToString16(id);
  DCHECK_LE(value.size(), 8U);
  string16 label(key);
  label += ASCIIToUTF16("=");
  label += value;
  label += ASCIIToUTF16("|");
  label += installer::BuildExperimentDateString();
  return label;
}

}  // namespace

void GetFieldTrialActiveGroupIds(
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  // A note on thread safety: Since GetActiveFieldTrialGroups() is thread
  // safe, and we operate on a separate list of that data, this function is
  // technically thread safe as well, with respect to the FieldTriaList data.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  GetFieldTrialActiveGroupIdsForActiveGroups(active_groups,
                                             name_group_ids);
}

void GetFieldTrialActiveGroupIdsAsStrings(
    std::vector<string16>* output) {
  DCHECK(output->empty());
  std::vector<ActiveGroupId> name_group_ids;
  GetFieldTrialActiveGroupIds(&name_group_ids);
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    output->push_back(UTF8ToUTF16(base::StringPrintf(
        "%x-%x", name_group_ids[i].name, name_group_ids[i].group)));
  }
}

void AssociateGoogleVariationID(IDCollectionKey key,
                                const std::string& trial_name,
                                const std::string& group_name,
                                chrome_variations::VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      key, MakeActiveGroupId(trial_name, group_name), id, false);
}

void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     const std::string& trial_name,
                                     const std::string& group_name,
                                     chrome_variations::VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      key, MakeActiveGroupId(trial_name, group_name), id, true);
}

chrome_variations::VariationID GetGoogleVariationID(
    IDCollectionKey key,
    const std::string& trial_name,
    const std::string& group_name) {
  return GroupMapAccessor::GetInstance()->GetID(
      key, MakeActiveGroupId(trial_name, group_name));
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
  GetFieldTrialActiveGroupIdsAsStrings(&experiment_strings);
  child_process_logging::SetExperimentList(experiment_strings);
}

string16 BuildGoogleUpdateExperimentLabel(
    base::FieldTrial::ActiveGroups& active_groups) {
  string16 experiment_labels;
  int counter = 0;

  // Find all currently active VariationIDs associated with Google Update.
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    const chrome_variations::VariationID id =
        chrome_variations::GetGoogleVariationID(
            chrome_variations::GOOGLE_UPDATE_SERVICE,
            it->trial_name, it->group_name);

    if (id == chrome_variations::EMPTY_ID)
      continue;

    if (!experiment_labels.empty())
      experiment_labels += ASCIIToUTF16(kExperimentLabelSep);
    experiment_labels += CreateSingleExperimentLabel(++counter, id);
  }

  return experiment_labels;
}

string16 ExtractNonVariationLabels(const string16& labels) {
  const string16 separator = ASCIIToUTF16(kExperimentLabelSep);
  string16 non_variation_labels;

  // First, split everything by the label separator.
  std::vector<string16> entries;
  base::SplitStringUsingSubstr(labels, separator, &entries);

  // For each label, keep the ones that do not look like a Variations label.
  for (std::vector<string16>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (it->empty() || StartsWith(*it, ASCIIToUTF16(kVariationPrefix), false))
      continue;

    // Dump the whole thing, including the timestamp.
    if (!non_variation_labels.empty())
      non_variation_labels += separator;
    non_variation_labels += *it;
  }

  return non_variation_labels;
}

string16 CombineExperimentLabels(const string16& variation_labels,
                                 const string16& other_labels) {
  const string16 separator = ASCIIToUTF16(kExperimentLabelSep);
  DCHECK(!StartsWith(variation_labels, separator, false));
  DCHECK(!EndsWith(variation_labels, separator, false));
  DCHECK(!StartsWith(other_labels, separator, false));
  DCHECK(!EndsWith(other_labels, separator, false));
  // Note that if either label is empty, a separator is not necessary.
  string16 combined_labels = other_labels;
  if (!other_labels.empty() && !variation_labels.empty())
    combined_labels += separator;
  combined_labels += variation_labels;
  return combined_labels;
}

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace testing {

void ClearAllVariationIDs() {
  GroupMapAccessor::GetInstance()->ClearAllMapsForTesting();
}

void TestGetFieldTrialActiveGroupIds(
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  GetFieldTrialActiveGroupIdsForActiveGroups(active_groups,
                                             name_group_ids);
}

}  // namespace testing

}  // namespace chrome_variations

