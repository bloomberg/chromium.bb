// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_REMOTE_CONTROLLER_H_
#define CHROME_BROWSER_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_REMOTE_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/metrics/leak_detector/leak_detector.mojom.h"
#include "components/metrics/proto/memory_leak_report.pb.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace metrics {

// This class acts as an interface to LeakDetector clients running on processes
// other than its own (the browser process).
class LeakDetectorRemoteController : public mojom::LeakDetector {
 public:
  // Interface class to be implemented by a local controller class that provides
  // leak detector parameters and can pass leak reports to UMA.
  class LocalController {
   public:
    virtual ~LocalController() {}

    // Returns a set of leak detection params to be used when initializing the
    // leak detector on a remote process. The controller may vary the parameters
    // between each call to this function, and to change its internal state.
    // Hence this is function is not const.
    virtual MemoryLeakReportProto::Params GetParamsAndRecordRequest() = 0;

    // Pass a vector of memory leak reports provided by a remote process to the
    // local controller class.
    virtual void SendLeakReports(
        const std::vector<MemoryLeakReportProto>& reports) = 0;

    // Signal that a remote process that had been running the leak detector
    // (i.e. been given params with sampling_rate = 0).
    virtual void OnRemoteProcessShutdown() = 0;
  };

  ~LeakDetectorRemoteController() override;

  static void Create(mojom::LeakDetectorRequest request);

  // mojom::LeakDetector:
  void GetParams(
      const mojom::LeakDetector::GetParamsCallback& callback) override;
  void SendLeakReports(std::vector<mojom::MemoryLeakReportPtr>) override;

  // Sets a global pointer to a LocalController implementation. The global
  // pointer is defined in the .cc file.
  static void SetLocalControllerInstance(LocalController* controller);

 private:
  LeakDetectorRemoteController();

  // Gets called when the remote process terminates and the Mojo connection gets
  // closed as a result.
  void OnRemoteProcessShutdown();

  // Indicates whether remote process received MemoryLeakReportProto::Params
  // with a non-zero sampling rate, i.e. enabled leak detector.
  bool leak_detector_enabled_on_remote_process_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorRemoteController);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_REMOTE_CONTROLLER_H_
