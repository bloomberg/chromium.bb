// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_channel_status_syncer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "components/gcm_driver/gcm_channel_status_request.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace gcm {

namespace {

// The GCM channel's enabled state.
const char kGCMChannelStatus[] = "gcm.channel_status";

// The GCM channel's polling interval (in seconds).
const char kGCMChannelPollIntervalSeconds[] = "gcm.poll_interval";

// Last time when checking with the GCM channel status server is done.
const char kGCMChannelLastCheckTime[] = "gcm.check_time";

// A small delay to avoid sending request at browser startup time for first-time
// request.
const int kFirstTimeDelaySeconds = 1 * 60;  // 1 minute.

// The fuzzing variation added to the polling delay.
const int kGCMChannelRequestTimeJitterSeconds = 15 * 60;  // 15 minues.

}  // namespace

// static
void GCMChannelStatusSyncer::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kGCMChannelStatus, true);
  registry->RegisterIntegerPref(
      kGCMChannelPollIntervalSeconds,
      GCMChannelStatusRequest::default_poll_interval_seconds());
  registry->RegisterInt64Pref(kGCMChannelLastCheckTime, 0);
}

// static
void GCMChannelStatusSyncer::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      kGCMChannelStatus,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kGCMChannelPollIntervalSeconds,
      GCMChannelStatusRequest::default_poll_interval_seconds(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      kGCMChannelLastCheckTime,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
int GCMChannelStatusSyncer::first_time_delay_seconds() {
  return kFirstTimeDelaySeconds;
}

GCMChannelStatusSyncer::GCMChannelStatusSyncer(
    GCMDriver* driver,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : driver_(driver),
      prefs_(prefs),
      request_context_(request_context),
      gcm_enabled_(true),
      poll_interval_seconds_(
          GCMChannelStatusRequest::default_poll_interval_seconds()),
      delay_removed_for_testing_(false),
      weak_ptr_factory_(this) {
  gcm_enabled_ = prefs_->GetBoolean(kGCMChannelStatus);
  poll_interval_seconds_ = prefs_->GetInteger(kGCMChannelPollIntervalSeconds);
  if (poll_interval_seconds_ <
      GCMChannelStatusRequest::min_poll_interval_seconds()) {
    poll_interval_seconds_ =
        GCMChannelStatusRequest::min_poll_interval_seconds();
  }
  last_check_time_ = base::Time::FromInternalValue(
      prefs_->GetInt64(kGCMChannelLastCheckTime));
}

GCMChannelStatusSyncer::~GCMChannelStatusSyncer() {
}

void GCMChannelStatusSyncer::EnsureStarted() {
  // Bail out if the request is already scheduled or started.
  if (weak_ptr_factory_.HasWeakPtrs() || request_)
    return;

  ScheduleRequest();
}

void GCMChannelStatusSyncer::Stop() {
  request_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GCMChannelStatusSyncer::OnRequestCompleted(bool enabled,
                                                int poll_interval_seconds) {
  DCHECK(request_);
  request_.reset();

  // Persist the current time as the last request complete time.
  last_check_time_ = base::Time::Now();
  prefs_->SetInt64(kGCMChannelLastCheckTime,
                   last_check_time_.ToInternalValue());

  if (gcm_enabled_ != enabled) {
    gcm_enabled_ = enabled;
    prefs_->SetBoolean(kGCMChannelStatus, enabled);
    if (gcm_enabled_)
      driver_->Enable();
    else
      driver_->Disable();
  }

  DCHECK_GE(poll_interval_seconds,
            GCMChannelStatusRequest::min_poll_interval_seconds());
  if (poll_interval_seconds_ != poll_interval_seconds) {
    poll_interval_seconds_ = poll_interval_seconds;
    prefs_->SetInteger(kGCMChannelPollIntervalSeconds, poll_interval_seconds_);
  }

  ScheduleRequest();
}

void GCMChannelStatusSyncer::ScheduleRequest() {
  current_request_delay_interval_ = GetRequestDelayInterval();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GCMChannelStatusSyncer::StartRequest,
                 weak_ptr_factory_.GetWeakPtr()),
      current_request_delay_interval_);
}

void GCMChannelStatusSyncer::StartRequest() {
  DCHECK(!request_);

  request_.reset(new GCMChannelStatusRequest(
      request_context_,
      base::Bind(&GCMChannelStatusSyncer::OnRequestCompleted,
                 weak_ptr_factory_.GetWeakPtr())));
  request_->Start();
}

base::TimeDelta GCMChannelStatusSyncer::GetRequestDelayInterval() const {
  // No delay during testing.
  if (delay_removed_for_testing_)
    return base::TimeDelta();

  // Make sure that checking with server occurs at polling interval, regardless
  // whether the browser restarts.
  int64 delay_seconds = poll_interval_seconds_ -
      (base::Time::Now() - last_check_time_).InSeconds();
  if (delay_seconds < 0)
    delay_seconds = 0;

  if (last_check_time_.is_null()) {
    // For the first-time request, add a small delay to avoid sending request at
    // browser startup time.
    DCHECK(!delay_seconds);
    delay_seconds = kFirstTimeDelaySeconds;
  } else {
    // Otherwise, add a fuzzing variation to the delay.
    delay_seconds += base::RandInt(0, kGCMChannelRequestTimeJitterSeconds);
  }

  return base::TimeDelta::FromSeconds(delay_seconds);
}

}  // namespace gcm
