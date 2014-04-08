// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/metrics/perf_provider_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"

namespace {

// Default time in seconds between invocations of perf.
// This period is roughly 6.5 hours.
// This is chosen to be relatively prime with the number of seconds in:
// - one minute (60)
// - one hour (3600)
// - one day (86400)
const size_t kPerfCommandIntervalDefaultSeconds = 23093;

// The first collection interval is different from the interval above. This is
// because we want to collect the first profile quickly after Chrome is started.
// If this period is too long, the user will log off and Chrome will be killed
// before it is triggered. The following 2 variables determine the upper and
// lower bound on the interval.
// The reason we do not always want to collect the initial profile after a fixed
// period is to not over-represent task X in the profile where task X always
// runs at a fixed period after start-up. By selecting a period randomly between
// a lower and upper bound, we will hopefully collect a more fair profile.
const size_t kPerfCommandStartIntervalLowerBoundMinutes = 10;

const size_t kPerfCommandStartIntervalUpperBoundMinutes = 20;

// Default time in seconds perf is run for.
const size_t kPerfCommandDurationDefaultSeconds = 2;

// Limit the total size of protobufs that can be cached, so they don't take up
// too much memory. If the size of cached protobufs exceeds this value, stop
// collecting further perf data. The current value is 4 MB.
const size_t kCachedPerfDataProtobufSizeThreshold = 4 * 1024 * 1024;

// Enumeration representing success and various failure modes for collecting and
// sending perf data.
enum GetPerfDataOutcome {
  SUCCESS,
  NOT_READY_TO_UPLOAD,
  NOT_READY_TO_COLLECT,
  INCOGNITO_ACTIVE,
  INCOGNITO_LAUNCHED,
  PROTOBUF_NOT_PARSED,
  NUM_OUTCOMES
};

// Name of the histogram that represents the success and various failure modes
// for collecting and sending perf data.
const char kGetPerfDataOutcomeHistogram[] = "UMA.Perf.GetData";

void AddToPerfHistogram(GetPerfDataOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(kGetPerfDataOutcomeHistogram,
                            outcome,
                            NUM_OUTCOMES);
}

}  // namespace


namespace metrics {

// This class must be created and used on the UI thread. It watches for any
// incognito window being opened from the time it is instantiated to the time it
// is destroyed.
class WindowedIncognitoObserver : public chrome::BrowserListObserver {
 public:
  WindowedIncognitoObserver() : incognito_launched_(false) {
    BrowserList::AddObserver(this);
  }

  virtual ~WindowedIncognitoObserver() {
    BrowserList::RemoveObserver(this);
  }

  // This method can be checked to see whether any incognito window has been
  // opened since the time this object was created.
  bool incognito_launched() {
    return incognito_launched_;
  }

 private:
  // chrome::BrowserListObserver implementation.
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE {
    if (browser->profile()->IsOffTheRecord())
      incognito_launched_ = true;
  }

  bool incognito_launched_;
};

PerfProvider::PerfProvider()
      : weak_factory_(this) {
  size_t collection_interval_minutes = base::RandInt(
      kPerfCommandStartIntervalLowerBoundMinutes,
      kPerfCommandStartIntervalUpperBoundMinutes);
  ScheduleCollection(base::TimeDelta::FromMinutes(collection_interval_minutes));
}

PerfProvider::~PerfProvider() {}

bool PerfProvider::GetPerfData(std::vector<PerfDataProto>* perf_data) {
  DCHECK(CalledOnValidThread());
  if (cached_perf_data_.empty()) {
    AddToPerfHistogram(NOT_READY_TO_UPLOAD);
    return false;
  }

  perf_data->swap(cached_perf_data_);
  cached_perf_data_.clear();

  AddToPerfHistogram(SUCCESS);
  return true;
}

void PerfProvider::ScheduleCollection(const base::TimeDelta& interval) {
  DCHECK(CalledOnValidThread());
  if (timer_.IsRunning())
    return;

  timer_.Start(FROM_HERE, interval, this,
               &PerfProvider::CollectIfNecessaryAndReschedule);
}

void PerfProvider::CollectIfNecessary() {
  DCHECK(CalledOnValidThread());

  // Do not collect further data if we've already collected a substantial amount
  // of data, as indicated by |kCachedPerfDataProtobufSizeThreshold|.
  size_t cached_perf_data_size = 0;
  for (size_t i = 0; i < cached_perf_data_.size(); ++i) {
    cached_perf_data_size += cached_perf_data_[i].ByteSize();
  }
  if (cached_perf_data_size >= kCachedPerfDataProtobufSizeThreshold) {
    AddToPerfHistogram(NOT_READY_TO_COLLECT);
    return;
  }

  // For privacy reasons, Chrome should only collect perf data if there is no
  // incognito session active (or gets spawned during the collection).
  if (BrowserList::IsOffTheRecordSessionActive()) {
    AddToPerfHistogram(INCOGNITO_ACTIVE);
    return;
  }

  scoped_ptr<WindowedIncognitoObserver> incognito_observer(
      new WindowedIncognitoObserver);

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

  base::TimeDelta collection_duration = base::TimeDelta::FromSeconds(
      kPerfCommandDurationDefaultSeconds);

  client->GetPerfData(collection_duration.InSeconds(),
                      base::Bind(&PerfProvider::ParseProtoIfValid,
                                 weak_factory_.GetWeakPtr(),
                                 base::Passed(&incognito_observer)));
}

void PerfProvider::CollectIfNecessaryAndReschedule() {
  CollectIfNecessary();
  ScheduleCollection(
      base::TimeDelta::FromSeconds(kPerfCommandIntervalDefaultSeconds));
}

void PerfProvider::ParseProtoIfValid(
    scoped_ptr<WindowedIncognitoObserver> incognito_observer,
    const std::vector<uint8>& data) {
  DCHECK(CalledOnValidThread());

  if (incognito_observer->incognito_launched()) {
    AddToPerfHistogram(INCOGNITO_LAUNCHED);
    return;
  }

  PerfDataProto perf_data_proto;
  if (!perf_data_proto.ParseFromArray(data.data(), data.size())) {
    AddToPerfHistogram(PROTOBUF_NOT_PARSED);
    return;
  }

  // Append a new PerfDataProto to the |cached_perf_data_| vector and swap in
  // the contents.
  cached_perf_data_.resize(cached_perf_data_.size() + 1);
  cached_perf_data_.back().Swap(&perf_data_proto);
}

}  // namespace metrics
