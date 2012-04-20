// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_EXPERIMENTS_HELPER_H_
#define CHROME_COMMON_METRICS_EXPERIMENTS_HELPER_H_
#pragma once

#include "base/metrics/field_trial.h"

// This namespace provides various helpers that extend the functionality around
// base::FieldTrial.
//
// This includes a simple API used to handle getting and setting
// data related to Google-specific experiments in the browser. This is meant to
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
// // All groups are now created. We want to associate GoogleExperimentIDs with
// // them, so do that now.
// AssociateGoogleExperimentID(
//     FieldTrial::MakeNameGroupId("trial", "default"), 123);
// AssociateGoogleExperimentID(
//     FieldTrial::MakeNameGroupId("trial", "HighMem"), 456);
// AssociateGoogleExperimentID(
//     FieldTrial::MakeNameGroupId("trial", "LowMem"), 789);
//
// // Elsewhere, we are interested in retrieving the GoogleExperimentID
// // assocaited with |trial|.
// GoogleExperimentID id = GetGoogleExperimentID(
//     FieldTrial::MakeNameGroupId(trial->name(), trial->group_name()));
// // Do stuff with |id|...
//
// The AssociateGoogleExperimentID and GetGoogleExperimentID API methods are
// thread safe.
namespace experiments_helper {

// An ID used by Google servers to identify a local browser experiment.
typedef uint32 GoogleExperimentID;

// Used to represent no associated Google experiment ID. Calls to the
// GetGoogleExperimentID API below will return this empty value for FieldTrial
// groups uninterested in associating themselves with Google experiments, or
// those that have not yet been seen yet.
extern const GoogleExperimentID kEmptyGoogleExperimentID;

// Set the GoogleExperimentID associated with a FieldTrial group. The group is
// denoted by |group_identifier|, which can be created by passing the
// FieldTrial's trial and group names to base::FieldTrial::MakeNameGroupId.
// This does not need to be called for FieldTrials uninterested in Google
// experiments.
void AssociateGoogleExperimentID(
    const base::FieldTrial::NameGroupId& group_identifier,
    GoogleExperimentID id);

// Retrieve the GoogleExperimentID associated with a FieldTrial group. The group
// is denoted by |group_identifier| (see comment above). This can be nicely
// combined with FieldTrial::GetFieldTrialNameGroupIds to enumerate the
// GoogleExperimentIDs for all active FieldTrial groups.
GoogleExperimentID GetGoogleExperimentID(
    const base::FieldTrial::NameGroupId& group_identifier);

// Get the current set of chosen FieldTrial groups (aka experiments) and send
// them to the child process logging module so it can save it for crash dumps.
void SetChildProcessLoggingExperimentList();

}  // namespace experiments_helper

#endif  // CHROME_COMMON_METRICS_EXPERIMENTS_HELPER_H_
