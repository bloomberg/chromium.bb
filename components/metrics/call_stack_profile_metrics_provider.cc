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

// Cap the number of pending unserialized profiles to avoid excessive memory
// usage when profile uploads are delayed (e.g., due to being offline). When the
// number of pending unserialized profiles exceeds this cap, serialize all
// additional unserialized profiles to save memory. Since profile serialization
// and deserialization (required later for uploads) are expensive, we choose 250
// as the capacity to balance speed and memory. 250 unserialized profiles
// corresponds to 16MB of storage.
// TODO(chengx): Remove this threshold after moving to a more memory-efficient
// profile representation.
constexpr size_t kMaxPendingUnserializedProfiles = 250;

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

  // Retrieves all the pending unserialized profiles.
  std::vector<SampledProfile> RetrieveUnserializedProfiles();

  // Retrieves all the pending serialized profiles.
  std::vector<std::string> RetrieveSerializedProfiles();

  // Enables the collection of profiles by CollectUnserializedProfile or
  // CollectSerializedProfile if |enabled| is true. Otherwise, clears current
  // profiles and ignores profiles provided to future invocations of
  // CollectUnserializedProfile or CollectSerializedProfile.
  void SetCollectionEnabled(bool enabled);

  // Returns true if collection is enabled for a given profile based on its
  // |profile_start_time|.
  bool IsCollectionEnabledForProfile(base::TimeTicks profile_start_time) const;

  // Collects |profile|. It may be stored as it is, or in a serialized form, or
  // ignored, depending on the pre-defined storage capacity; it is not const&
  // because it must be passed with std::move.
  void CollectUnserializedProfile(SampledProfile profile);

  // Collects |serialized_profile|. It may be ignored depending on the
  // pre-defined storage capacity; it is not const& because it must be passed
  // with std::move.
  void CollectSerializedProfile(std::string serialized_profile);

  // Allows testing against the initial state multiple times.
  void ResetToDefaultStateForTesting();

 private:
  friend class base::NoDestructor<PendingProfiles>;

  PendingProfiles();
  ~PendingProfiles() = delete;

  mutable base::Lock lock_;

  // If true, profiles provided to CollectUnserializedProfile or
  // CollectSerializedProfile should be collected. Otherwise they will be
  // ignored.
  bool collection_enabled_;

  // The last time collection was disabled. Used to determine if collection was
  // disabled at any point since a profile was started.
  base::TimeTicks last_collection_disable_time_;

  // The last time collection was enabled. Used to determine if collection was
  // enabled at any point since a profile was started.
  base::TimeTicks last_collection_enable_time_;

  // The set of completed unserialized profiles that should be reported.
  std::vector<SampledProfile> unserialized_profiles_;

  // The set of completed serialized profiles that should be reported.
  std::vector<std::string> serialized_profiles_;

  DISALLOW_COPY_AND_ASSIGN(PendingProfiles);
};

// static
PendingProfiles* PendingProfiles::GetInstance() {
  // Singleton for performance rather than correctness reasons.
  static base::NoDestructor<PendingProfiles> instance;
  return instance.get();
}

std::vector<SampledProfile> PendingProfiles::RetrieveUnserializedProfiles() {
  base::AutoLock scoped_lock(lock_);
  return std::move(unserialized_profiles_);
}

std::vector<std::string> PendingProfiles::RetrieveSerializedProfiles() {
  base::AutoLock scoped_lock(lock_);
  return std::move(serialized_profiles_);
}

void PendingProfiles::SetCollectionEnabled(bool enabled) {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = enabled;

  if (!collection_enabled_) {
    unserialized_profiles_.clear();
    serialized_profiles_.clear();
    last_collection_disable_time_ = base::TimeTicks::Now();
  } else {
    last_collection_enable_time_ = base::TimeTicks::Now();
  }
}

bool PendingProfiles::IsCollectionEnabledForProfile(
    base::TimeTicks profile_start_time) const {
  base::AutoLock scoped_lock(lock_);

  // Scenario 1: return false if collection is disabled.
  if (!collection_enabled_)
    return false;

  // Scenario 2: return false if collection is disabled after the start of
  // collection for this profile.
  if (!last_collection_disable_time_.is_null() &&
      last_collection_disable_time_ >= profile_start_time) {
    return false;
  }

  // Scenario 3: return false if collection is disabled before the start of
  // collection and re-enabled after the start. Note that this is different from
  // scenario 1 where re-enabling never happens.
  if (!last_collection_disable_time_.is_null() &&
      !last_collection_enable_time_.is_null() &&
      last_collection_enable_time_ >= profile_start_time) {
    return false;
  }

  return true;
}

void PendingProfiles::CollectUnserializedProfile(SampledProfile profile) {
  base::AutoLock scoped_lock(lock_);

  // Store the unserialized profile directly if we are under the capacity for
  // unserialized profiles.
  if (unserialized_profiles_.size() < kMaxPendingUnserializedProfiles) {
    unserialized_profiles_.push_back(std::move(profile));
    return;
  }

  // If there is no room for unserialized profiles but there is still room for
  // serialized profiles, convert the unserialized profile to serialized form.
  if ((unserialized_profiles_.size() + serialized_profiles_.size()) <
      kMaxPendingProfiles) {
    std::string serialized_profile;
    profile.SerializeToString(&serialized_profile);
    serialized_profiles_.push_back(std::move(serialized_profile));
  }
}

void PendingProfiles::CollectSerializedProfile(std::string serialized_profile) {
  base::AutoLock scoped_lock(lock_);

  if ((unserialized_profiles_.size() + serialized_profiles_.size()) <
      kMaxPendingProfiles) {
    serialized_profiles_.push_back(std::move(serialized_profile));
  }
}

void PendingProfiles::ResetToDefaultStateForTesting() {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = true;
  last_collection_disable_time_ = base::TimeTicks();
  last_collection_enable_time_ = base::TimeTicks();
  unserialized_profiles_.clear();
  serialized_profiles_.clear();
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
  if (!PendingProfiles::GetInstance()->IsCollectionEnabledForProfile(
          profile_start_time)) {
    return;
  }

  PendingProfiles::GetInstance()->CollectUnserializedProfile(
      std::move(profile));
}

// static
void CallStackProfileMetricsProvider::ReceiveSerializedProfile(
    base::TimeTicks profile_start_time,
    std::string serialized_profile) {
  if (!PendingProfiles::GetInstance()->IsCollectionEnabledForProfile(
          profile_start_time)) {
    return;
  }

  PendingProfiles::GetInstance()->CollectSerializedProfile(
      std::move(serialized_profile));
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
  std::vector<SampledProfile> unserialized_profiles =
      PendingProfiles::GetInstance()->RetrieveUnserializedProfiles();

  std::vector<std::string> serialized_profiles =
      PendingProfiles::GetInstance()->RetrieveSerializedProfiles();

  DCHECK(base::FeatureList::IsEnabled(kEnableReporting) ||
         (unserialized_profiles.empty() && serialized_profiles.empty()));

  for (auto& profile : unserialized_profiles)
    *uma_proto->add_sampled_profile() = std::move(profile);

  for (auto& serialized_profile : serialized_profiles) {
    SampledProfile profile;
    if (profile.ParseFromArray(serialized_profile.data(),
                               serialized_profile.size())) {
      *uma_proto->add_sampled_profile() = std::move(profile);
    }
  }
}

// static
void CallStackProfileMetricsProvider::ResetStaticStateForTesting() {
  PendingProfiles::GetInstance()->ResetToDefaultStateForTesting();
}

}  // namespace metrics
