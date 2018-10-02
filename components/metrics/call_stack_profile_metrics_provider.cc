// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

namespace {

// Cap the number of pending profiles to avoid excessive memory usage when
// profile uploads are delayed (e.g. due to being offline). 1250 profiles
// corresponds to 80MB of storage. Capping at this threshold loses approximately
// 0.5% of profiles on canary and dev.
// TODO(chengx): Remove this threshold after moving to a more memory-efficient
// profile representation.
const size_t kMaxPendingProfiles = 1250;

// ProfileState --------------------------------------------------------------

// A set of profiles and the start time of the collection associated with them.
struct ProfileState {
  ProfileState(base::TimeTicks start_timestamp, SampledProfile profile);
  ProfileState(base::TimeTicks start_timestamp, std::string serialized_profile);
  ProfileState(ProfileState&&);
  ProfileState& operator=(ProfileState&&);

  // The time at which the profile collection was started.
  base::TimeTicks start_timestamp;

  // A serialized SampledProfile. The profile for this instance will be
  // contained in exactly one of |serialized_profile| or |profile|. If
  // |serialized_profile| is empty, the profile will be in |profile|.
  std::string serialized_profile;

  // The call stack profile message collected by the profiler.
  SampledProfile profile;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileState);
};

ProfileState::ProfileState(base::TimeTicks start_timestamp,
                           SampledProfile profile)
    : start_timestamp(start_timestamp), profile(std::move(profile)) {}

ProfileState::ProfileState(base::TimeTicks start_timestamp,
                           std::string serialized_profile)
    : start_timestamp(start_timestamp),
      serialized_profile(std::move(serialized_profile)) {}

ProfileState::ProfileState(ProfileState&&) = default;

// Some versions of GCC need this for push_back to work with std::move.
ProfileState& ProfileState::operator=(ProfileState&&) = default;

// PendingProfiles ------------------------------------------------------------

// Singleton class responsible for retaining profiles received from
// CallStackProfileBuilder. These are then sent to UMA on the invocation of
// CallStackProfileMetricsProvider::ProvideCurrentSessionData(). We need to
// store the profiles outside of a CallStackProfileMetricsProvider instance
// since callers may start profiling before the CallStackProfileMetricsProvider
// is created.
//
// Member functions on this class may be called on any thread.
class PendingProfiles {
 public:
  static PendingProfiles* GetInstance();

  // Retrieves all the pending profiles.
  std::vector<ProfileState> Retrieve();

  // Enables the collection of profiles by CollectProfilesIfCollectionEnabled if
  // |enabled| is true. Otherwise, clears current profiles and ignores profiles
  // provided to future invocations of CollectProfilesIfCollectionEnabled.
  void SetCollectionEnabled(bool enabled);

  // Adds |profile| to the list of profiles if collection is enabled; it is
  // not const& because it must be passed with std::move.
  void CollectProfilesIfCollectionEnabled(ProfileState profile);

  // Allows testing against the initial state multiple times.
  void ResetToDefaultStateForTesting();

 private:
  friend class base::NoDestructor<PendingProfiles>;

  PendingProfiles();
  ~PendingProfiles() = delete;

  mutable base::Lock lock_;

  // If true, profiles provided to CollectProfilesIfCollectionEnabled should be
  // collected. Otherwise they will be ignored.
  bool collection_enabled_;

  // The last time collection was disabled. Used to determine if collection was
  // disabled at any point since a profile was started.
  base::TimeTicks last_collection_disable_time_;

  // The last time collection was enabled. Used to determine if collection was
  // enabled at any point since a profile was started.
  base::TimeTicks last_collection_enable_time_;

  // The set of completed profiles that should be reported.
  std::vector<ProfileState> profiles_;

  DISALLOW_COPY_AND_ASSIGN(PendingProfiles);
};

// static
PendingProfiles* PendingProfiles::GetInstance() {
  // Singleton for performance rather than correctness reasons.
  static base::NoDestructor<PendingProfiles> instance;
  return instance.get();
}

std::vector<ProfileState> PendingProfiles::Retrieve() {
  base::AutoLock scoped_lock(lock_);
  return std::move(profiles_);
}

void PendingProfiles::SetCollectionEnabled(bool enabled) {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = enabled;

  if (!collection_enabled_) {
    profiles_.clear();
    last_collection_disable_time_ = base::TimeTicks::Now();
  } else {
    last_collection_enable_time_ = base::TimeTicks::Now();
  }
}

void PendingProfiles::CollectProfilesIfCollectionEnabled(ProfileState profile) {
  base::AutoLock scoped_lock(lock_);

  // Scenario 1: stop collection if it is disabled.
  if (!collection_enabled_)
    return;

  // Scenario 2: stop collection if it is disabled after the start of collection
  // for this profile.
  if (!last_collection_disable_time_.is_null() &&
      last_collection_disable_time_ >= profile.start_timestamp) {
    return;
  }

  // Scenario 3: stop collection if it is disabled before the start of
  // collection and re-enabled after the start. Note that this is different from
  // scenario 1 where re-enabling never happens.
  if (!last_collection_disable_time_.is_null() &&
      !last_collection_enable_time_.is_null() &&
      last_collection_enable_time_ >= profile.start_timestamp) {
    return;
  }

  if (profiles_.size() < kMaxPendingProfiles)
    profiles_.push_back(std::move(profile));
}

void PendingProfiles::ResetToDefaultStateForTesting() {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = true;
  last_collection_disable_time_ = base::TimeTicks();
  last_collection_enable_time_ = base::TimeTicks();
  profiles_.clear();
}

// |collection_enabled_| is initialized to true to collect any profiles that are
// generated prior to creation of the CallStackProfileMetricsProvider. The
// ultimate disposition of these pre-creation collected profiles will be
// determined by the initial recording state provided to
// CallStackProfileMetricsProvider.
PendingProfiles::PendingProfiles() : collection_enabled_(true) {}

}  // namespace

// CallStackProfileMetricsProvider --------------------------------------------

const base::Feature CallStackProfileMetricsProvider::kEnableReporting = {
    "SamplingProfilerReporting", base::FEATURE_DISABLED_BY_DEFAULT};

CallStackProfileMetricsProvider::CallStackProfileMetricsProvider() {}

CallStackProfileMetricsProvider::~CallStackProfileMetricsProvider() {}

// static
void CallStackProfileMetricsProvider::ReceiveProfile(
    base::TimeTicks profile_start_time,
    SampledProfile profile) {
  PendingProfiles::GetInstance()->CollectProfilesIfCollectionEnabled(
      ProfileState(profile_start_time, std::move(profile)));
  // TODO(wittman): Check if we have a lot of raw profiles outstanding
  // (e.g. because the client is offline) and if so, convert them to serialized
  // form to minimize memory usage.
}

// static
void CallStackProfileMetricsProvider::ReceiveSerializedProfile(
    base::TimeTicks profile_start_time,
    std::string serialized_sampled_profile) {
  PendingProfiles::GetInstance()->CollectProfilesIfCollectionEnabled(
      ProfileState(profile_start_time, std::move(serialized_sampled_profile)));
}

void CallStackProfileMetricsProvider::OnRecordingEnabled() {
  PendingProfiles::GetInstance()->SetCollectionEnabled(
      base::FeatureList::IsEnabled(kEnableReporting));
}

void CallStackProfileMetricsProvider::OnRecordingDisabled() {
  PendingProfiles::GetInstance()->SetCollectionEnabled(false);
}

void CallStackProfileMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  std::vector<ProfileState> pending_profiles =
      PendingProfiles::GetInstance()->Retrieve();

  DCHECK(base::FeatureList::IsEnabled(kEnableReporting) ||
         pending_profiles.empty());

  for (const auto& profile_state : pending_profiles) {
    if (!profile_state.serialized_profile.empty()) {
      SampledProfile profile;
      if (profile.ParseFromArray(profile_state.serialized_profile.data(),
                                 profile_state.serialized_profile.size())) {
        *uma_proto->add_sampled_profile() = std::move(profile);
      }
    } else {
      *uma_proto->add_sampled_profile() = std::move(profile_state.profile);
    }
  }
}

// static
void CallStackProfileMetricsProvider::ResetStaticStateForTesting() {
  PendingProfiles::GetInstance()->ResetToDefaultStateForTesting();
}

}  // namespace metrics
