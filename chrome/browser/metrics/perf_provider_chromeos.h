// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_PERF_PROVIDER_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/common/metrics/proto/perf_data.pb.h"

namespace metrics {

class WindowedIncognitoObserver;

// Provides access to ChromeOS perf data. perf aka "perf events" is a
// performance profiling infrastructure built into the linux kernel. For more
// information, see: https://perf.wiki.kernel.org/index.php/Main_Page.
class PerfProvider : public base::NonThreadSafe {
 public:
  PerfProvider();
  ~PerfProvider();

  // Gets the collected perf data protobuf and writes it to |perf_data_proto|.
  // Returns true if it wrote to |perf_data_proto|.
  bool GetPerfData(PerfDataProto* perf_data_proto);

 private:
  enum PerfDataState {
    // Indicates that we are ready to collect perf data.
    READY_TO_COLLECT,

    // Indicates that we are ready to upload perf data.
    READY_TO_UPLOAD,
  };

  // Starts an internal timer to start collecting perf data.
  void ScheduleCollection();

  // Collects perf data if it has not been consumed by calling GetPerfData()
  // (see above).
  void CollectIfNecessary();

  // Parses a protobuf from the |data| passed in only if the
  // |incognito_observer| indicates that no incognito window had been opened
  // during the profile collection period.
  void ParseProtoIfValid(
      scoped_ptr<WindowedIncognitoObserver> incognito_observer,
      const std::vector<uint8>& data);

  // The internal state can be one of the enum values above.
  PerfDataState state_;

  // Protobuf that has the perf data.
  PerfDataProto perf_data_proto_;

  // For scheduling collection of perf data.
  base::RepeatingTimer<PerfProvider> timer_;

  // To pass around the "this" pointer across threads safely.
  base::WeakPtrFactory<PerfProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PerfProvider);
};

}  // namespace system

#endif  // CHROME_BROWSER_METRICS_PERF_PROVIDER_CHROMEOS_H_
