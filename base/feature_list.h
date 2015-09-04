// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_LIST_H_
#define BASE_FEATURE_LIST_H_

#include <map>
#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"

namespace base {

// Specifies whether a given feature is enabled or disabled by default.
enum FeatureState {
  FEATURE_DISABLED_BY_DEFAULT,
  FEATURE_ENABLED_BY_DEFAULT,
};

// The Feature struct is used to define the default state for a feature. See
// comment below for more details. There must only ever be one struct instance
// for a given feature name - generally defined as a constant global variable or
// file static.
struct BASE_EXPORT Feature {
  // The name of the feature. This should be unique to each feature and is used
  // for enabling/disabling features via command line flags and experiments.
  const char* const name;

  // The default state (i.e. enabled or disabled) for this feature.
  const FeatureState default_state;
};

// The FeatureList class is used to determine whether a given feature is on or
// off. It provides an authoritative answer, taking into account command-line
// overrides and experimental control.
//
// The basic use case is for any feature that can be toggled (e.g. through
// command-line or an experiment) to have a defined Feature struct, e.g.:
//
//   struct base::Feature kMyGreatFeature {
//     "MyGreatFeature", base::FEATURE_ENABLED_BY_DEFAULT
//   };
//
// Then, client code that wishes to query the state of the feature would check:
//
//   if (base::FeatureList::IsEnabled(kMyGreatFeature)) {
//     // Feature code goes here.
//   }
//
// Behind the scenes, the above call would take into account any command-line
// flags to enable or disable the feature, any experiments that may control it
// and finally its default state (in that order of priority), to determine
// whether the feature is on.
//
// Features can be explicitly forced on or off by specifying a list of comma-
// separated feature names via the following command-line flags:
//
//   --enable-features=Feature5,Feature7
//   --disable-features=Feature1,Feature2,Feature3
//
// After initialization (which should be done single-threaded), the FeatureList
// API is thread safe.
//
// Note: This class is a singleton, but does not use base/memory/singleton.h in
// order to have control over its initialization sequence. Specifically, the
// intended use is to create an instance of this class and fully initialize it,
// before setting it as the singleton for a process, via SetInstance().
class BASE_EXPORT FeatureList {
 public:
  FeatureList();
  ~FeatureList();

  // Initializes feature overrides via command-line flags |enable_features| and
  // |disable_features|, each of which is a comma-separated list of features to
  // enable or disable, respectively. If a feature appears on both lists, then
  // it will be disabled. Must only be invoked during the initialization phase
  // (before FinalizeInitialization() has been called).
  void InitializeFromCommandLine(const std::string& enable_features,
                                 const std::string& disable_features);

  // Returns whether the given |feature| is enabled. Must only be called after
  // the singleton instance has been registered via SetInstance(). Additionally,
  // a feature with a given name must only have a single corresponding Feature
  // struct, which is checked in builds with DCHECKs enabled.
  static bool IsEnabled(const Feature& feature);

  // Returns the singleton instance of FeatureList. Will return null until an
  // instance is registered via SetInstance().
  static FeatureList* GetInstance();

  // Registers the given |instance| to be the singleton feature list for this
  // process. This should only be called once and |instance| must not be null.
  static void SetInstance(scoped_ptr<FeatureList> instance);

  // Clears the previously-registered singleton instance for tests.
  static void ClearInstanceForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(FeatureListTest, CheckFeatureIdentity);

  // Specifies whether a feature override enables or disables the feature.
  enum OverrideState {
    OVERRIDE_DISABLE_FEATURE,
    OVERRIDE_ENABLE_FEATURE,
  };

  // Finalizes the initialization state of the FeatureList, so that no further
  // overrides can be registered. This is called by SetInstance() on the
  // singleton feature list that is being registered.
  void FinalizeInitialization();

  // Returns whether the given |feature| is enabled. This is invoked by the
  // public FeatureList::IsEnabled() static function on the global singleton.
  // Requires the FeatureList to have already been fully initialized.
  bool IsFeatureEnabled(const Feature& feature);

  // Registers an override for feature |feature_name|. The override specifies
  // whether the feature should be on or off (via |overridden_state|), which
  // will take precedence over the feature's default state.
  void RegisterOverride(const std::string& feature_name,
                        OverrideState overridden_state);

  // Verifies that there's only a single definition of a Feature struct for a
  // given feature name. Keeps track of the first seen Feature struct for each
  // feature. Returns false when called on a Feature struct with a different
  // address than the first one it saw for that feature name. Used only from
  // DCHECKs and tests.
  bool CheckFeatureIdentity(const Feature& feature);

  struct OverrideEntry {
    // The overridden enable (on/off) state of the feature.
    const OverrideState overridden_state;

    // TODO(asvitkine): Expand this as more support is added.

    explicit OverrideEntry(OverrideState overridden_state);
  };
  // Map from feature name to an OverrideEntry struct for the feature, if it
  // exists.
  std::map<std::string, OverrideEntry> overrides_;

  // Locked map that keeps track of seen features, to ensure a single feature is
  // only defined once. This verification is only done in builds with DCHECKs
  // enabled.
  Lock feature_identity_tracker_lock_;
  std::map<std::string, const Feature*> feature_identity_tracker_;

  // Whether this object has been fully initialized. This gets set to true as a
  // result of FinalizeInitialization().
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(FeatureList);
};

}  // namespace base

#endif  // BASE_FEATURE_LIST_H_
