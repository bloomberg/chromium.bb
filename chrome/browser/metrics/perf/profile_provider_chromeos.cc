// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include "base/allocator/buildflags.h"
#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "chrome/browser/metrics/perf/heap_collector.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/perf_events_collector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

#if BUILDFLAG(USE_NEW_TCMALLOC)
// Sampling factor for enabling the heap collector. The heap collector is
// enabled with a probability of 1 / <sampling_factor>. A value of 5 implies a
// 20% probability. A value of 1 implies a 100% probability. A value <= 0
// disables the heap collector.
constexpr base::FeatureParam<int> kSamplingFactorForEnablingHeapCollector{
    &kCWPHeapCollection, "SamplingFactorForEnablingHeapCollector", 0};
#endif

// Returns true if a normal user is logged in. Returns false otherwise (e.g. if
// logged in as a guest or as a kiosk app).
bool IsNormalUserLoggedIn() {
  return chromeos::LoginState::Get()->IsUserAuthenticated();
}

}  // namespace

ProfileProvider::ProfileProvider() : weak_factory_(this) {
  // Register a perf events collector.
  collectors_.push_back(std::make_unique<PerfCollector>());
}

ProfileProvider::~ProfileProvider() {
  chromeos::LoginState::Get()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void ProfileProvider::Init() {
#if BUILDFLAG(USE_NEW_TCMALLOC)
  if (base::FeatureList::IsEnabled(kCWPHeapCollection)) {
    // Register a heap collector with a probability obtained from field params.
    int sampling_factor = kSamplingFactorForEnablingHeapCollector.Get();
    if (sampling_factor > 0 && base::RandGenerator(sampling_factor) == 0) {
      collectors_.push_back(std::make_unique<HeapCollector>());
    }
  }
#endif

  for (auto& collector : collectors_) {
    collector->Init();
  }

  // Register as an observer of login state changes.
  chromeos::LoginState::Get()->AddObserver(this);

  // Register as an observer of power manager events.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);

  // Register as an observer of session restore.
  on_session_restored_callback_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
          &ProfileProvider::OnSessionRestoreDone, weak_factory_.GetWeakPtr()));

  // Check the login state. At the time of writing, this class is instantiated
  // before login. A subsequent login would activate the profiling. However,
  // that behavior may change in the future so that the user is already logged
  // when this class is instantiated. By calling LoggedInStateChanged() here,
  // ProfileProvider will recognize that the system is already logged in.
  LoggedInStateChanged();
}

bool ProfileProvider::GetSampledProfiles(
    std::vector<SampledProfile>* sampled_profiles) {
  bool result = false;
  for (auto& collector : collectors_) {
    bool written = collector->GetSampledProfiles(sampled_profiles);
    result = result || written;
  }
  return result;
}

void ProfileProvider::LoggedInStateChanged() {
  if (IsNormalUserLoggedIn()) {
    for (auto& collector : collectors_) {
      collector->OnUserLoggedIn();
    }
  } else {
    for (auto& collector : collectors_) {
      collector->Deactivate();
    }
  }
}

void ProfileProvider::SuspendDone(const base::TimeDelta& sleep_duration) {
  // A zero value for the suspend duration indicates that the suspend was
  // canceled. Do not collect anything if that's the case.
  if (sleep_duration.is_zero())
    return;

  // Do not collect a profile unless logged in. The system behavior when closing
  // the lid or idling when not logged in is currently to shut down instead of
  // suspending. But it's good to enforce the rule here in case that changes.
  if (!IsNormalUserLoggedIn())
    return;

  // Inform each collector that a successful suspend has completed.
  for (auto& collector : collectors_) {
    collector->SuspendDone(sleep_duration);
  }
}

void ProfileProvider::OnSessionRestoreDone(int num_tabs_restored) {
  // Do not collect a profile unless logged in as a normal user.
  if (!IsNormalUserLoggedIn())
    return;

  // Inform each collector of a session restore event.
  for (auto& collector : collectors_) {
    collector->OnSessionRestoreDone(num_tabs_restored);
  }
}

}  // namespace metrics
