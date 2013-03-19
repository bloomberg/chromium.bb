// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_VARIATIONS_VARIATIONS_UTIL_H_
#define CHROME_COMMON_METRICS_VARIATIONS_VARIATIONS_UTIL_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/string16.h"
#include "chrome/common/metrics/variations/variation_ids.h"

// This namespace provides various helpers that extend the functionality around
// base::FieldTrial.
//
// This includes a simple API used to handle getting and setting
// data related to Google-specific variations in the browser. This is meant to
// be an extension to the base::FieldTrial for Google-specific functionality.
//
// These calls are meant to be made directly after appending all your groups to
// a FieldTrial (for associating IDs) and any time after the group selection has
// been done (for retrieving IDs).
//
// Typical usage looks like this:
//
// // Set up your trial and groups as usual.
// FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
//     "trial", 1000, "default", 2012, 12, 31, NULL);
// const int kHighMemGroup = trial->AppendGroup("HighMem", 20);
// const int kLowMemGroup = trial->AppendGroup("LowMem", 20);
// // All groups are now created. We want to associate
// // chrome_variation::VariationIDs with them, so do that now.
// AssociateGoogleVariationID("trial", "default", chrome_variations::kValueA);
// AssociateGoogleVariationID("trial", "HighMem", chrome_variations::kValueB);
// AssociateGoogleVariationID("trial", "LowMem",  chrome_variations::kValueC);
//
// // Elsewhere, we are interested in retrieving the VariationID associated
// // with |trial|.
// chrome_variations::VariationID id =
//     GetGoogleVariationID(trial->name(), trial->group_name());
// // Do stuff with |id|...
//
// The AssociateGoogleVariationID and GetGoogleVariationID API methods are
// thread safe.

namespace chrome_variations {

// A key into the Associate/Get methods for VariationIDs. This is used to create
// separate ID associations for separate parties interested in VariationIDs.
enum IDCollectionKey {
  // This collection is used by Google web properties, transmitted through the
  // X-Chrome-Variations header.
  GOOGLE_WEB_PROPERTIES,
  // This collection is used by Google update services, transmitted through the
  // Google Update experiment labels.
  GOOGLE_UPDATE_SERVICE,
  // The total count of collections.
  ID_COLLECTION_COUNT,
};

// The Unique ID of a trial and its active group, where the name and group
// identifiers are hashes of the trial and group name strings.
struct ActiveGroupId {
  uint32 name;
  uint32 group;
};

// We need to supply a Compare class for templates since ActiveGroupId is a
// user-defined type.
struct ActiveGroupIdCompare {
  bool operator() (const ActiveGroupId& lhs,
                   const ActiveGroupId& rhs) const {
    // The group and name fields are just SHA-1 Hashes, so we just need to treat
    // them as IDs and do a less-than comparison. We test group first, since
    // name is more likely to collide.
    if (lhs.group != rhs.group)
      return lhs.group < rhs.group;
    return lhs.name < rhs.name;
  }
};

// Fills the supplied vector |name_group_ids| (which must be empty when called)
// with unique ActiveGroupIds for each Field Trial that has a chosen group.
// Field Trials for which a group has not been chosen yet are NOT returned in
// this list.
void GetFieldTrialActiveGroupIds(
    std::vector<ActiveGroupId>* name_group_ids);

// Fills the supplied vector |output| (which must be empty when called) with
// unique string representations of ActiveGroupIds for each Field Trial that
// has a chosen group. The strings are formatted as "<TrialName>-<GroupName>",
// with the names as hex strings. Field Trials for which a group has not been
// chosen yet are NOT returned in this list.
void GetFieldTrialActiveGroupIdsAsStrings(std::vector<string16>* output);

// Associate a chrome_variations::VariationID value with a FieldTrial group for
// collection |key|. If an id was previously set for |trial_name| and
// |group_name|, this does nothing. The group is denoted by |trial_name| and
// |group_name|. This must be called whenever a FieldTrial is prepared (create
// the trial and append groups) and needs to have a
// chrome_variations::VariationID associated with it so Google servers can
// recognize the FieldTrial.
void AssociateGoogleVariationID(IDCollectionKey key,
                                const std::string& trial_name,
                                const std::string& group_name,
                                chrome_variations::VariationID id);

// As above, but overwrites any previously set id.
void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     const std::string& trial_name,
                                     const std::string& group_name,
                                     chrome_variations::VariationID id);

// Retrieve the chrome_variations::VariationID associated with a FieldTrial
// group for collection |key|. The group is denoted by |trial_name| and
// |group_name|. This will return chrome_variations::kEmptyID if there is
// currently no associated ID for the named group. This API can be nicely
// combined with FieldTrial::GetActiveFieldTrialGroups() to enumerate the
// variation IDs for all active FieldTrial groups.
chrome_variations::VariationID GetGoogleVariationID(
    IDCollectionKey key,
    const std::string& trial_name,
    const std::string& group_name);

// Generates variation chunks from |variation_strings| that are suitable for
// crash reporting.
void GenerateVariationChunks(const std::vector<string16>& variation_strings,
                             std::vector<string16>* chunks);

// Get the current set of chosen FieldTrial groups (aka variations) and send
// them to the child process logging module so it can save it for crash dumps.
void SetChildProcessLoggingVariationList();

// Takes the list of active groups and builds the label for the ones that have
// Google Update VariationID associated with them. This will return an empty
// string if there are no such groups.
string16 BuildGoogleUpdateExperimentLabel(
    base::FieldTrial::ActiveGroups& active_groups);

// Creates a final combined experiment labels string with |variation_labels|
// and |other_labels|, appropriately appending a separator based on their
// contents. It is assumed that |variation_labels| and |other_labels| do not
// have leading or trailing separators.
string16 CombineExperimentLabels(const string16& variation_labels,
                                 const string16& other_labels);

// Takes the value of experiment_labels from the registry and returns a valid
// experiment_labels string value containing only the labels that are not
// associated with Chrome Variations.
string16 ExtractNonVariationLabels(const string16& labels);

// Expose some functions for testing. These functions just wrap functionality
// that is implemented above.
namespace testing {

// Clears all of the mapped associations.
void ClearAllVariationIDs();

void TestGetFieldTrialActiveGroupIds(
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids);

}  // namespace testing

}  // namespace chrome_variations

#endif  // CHROME_COMMON_METRICS_VARIATIONS_VARIATIONS_UTIL_H_
