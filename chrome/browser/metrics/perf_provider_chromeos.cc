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
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/metrics/perf_provider_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Default time in seconds between invocations of perf.
const unsigned kPerfCommandIntervalDefaultSeconds = 12 * 60 * 60;

// Default time in seconds perf is run for.
const unsigned kPerfCommandDurationDefaultSeconds = 2;

} // namespace


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
      : state_(READY_TO_COLLECT),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  ScheduleCollection();
}

PerfProvider::~PerfProvider() {}

bool PerfProvider::GetPerfData(PerfDataProto* perf_data_proto) {
  DCHECK(CalledOnValidThread());
  if (state_ != READY_TO_UPLOAD)
    return false;

  *perf_data_proto = perf_data_proto_;
  state_ = READY_TO_COLLECT;
  return true;
}

void PerfProvider::ScheduleCollection() {
  DCHECK(CalledOnValidThread());
  if (timer_.IsRunning())
    return;

  base::TimeDelta collection_interval = base::TimeDelta::FromSeconds(
      kPerfCommandIntervalDefaultSeconds);

  timer_.Start(FROM_HERE, collection_interval, this,
               &PerfProvider::CollectIfNecessary);
}

void PerfProvider::CollectIfNecessary() {
  DCHECK(CalledOnValidThread());
  if (state_ != READY_TO_COLLECT)
    return;

  // For privacy reasons, Chrome should only collect perf data if there is no
  // incognito session active (or gets spawned during the collection).
  if (BrowserList::IsOffTheRecordSessionActive())
    return;

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


void PerfProvider::ParseProtoIfValid(
    scoped_ptr<WindowedIncognitoObserver> incognito_observer,
    const std::vector<uint8>& data) {
  DCHECK(CalledOnValidThread());

  if (incognito_observer->incognito_launched())
    return;

  if (!perf_data_proto_.ParseFromArray(data.data(), data.size())) {
    perf_data_proto_.Clear();
    return;
  }

  state_ = READY_TO_UPLOAD;
}
} // namespace metrics
