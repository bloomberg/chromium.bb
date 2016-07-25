// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_CONTROLLER_H_
#define CHROME_BROWSER_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_CONTROLLER_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/metrics/leak_detector/leak_detector_remote_controller.h"
#include "components/metrics/leak_detector/leak_detector.h"
#include "components/metrics/proto/memory_leak_report.pb.h"

namespace metrics {

// This class initializes the LeakDetector on the browser process and registers
// itself to be notified of leak reports.
class LeakDetectorController
    : public LeakDetector::Observer,
      public LeakDetectorRemoteController::LocalController {
 public:
  LeakDetectorController();
  ~LeakDetectorController() override;

  // Retrieves all reports in |stored_reports_|, moving them to |*reports|.
  // |stored_reports_| is empty afterwards.
  void GetLeakReports(std::vector<MemoryLeakReportProto>* reports);

 protected:
  // LeakDetector::Observer:
  void OnLeaksFound(const std::vector<MemoryLeakReportProto>& reports) override;

  // LeakDetectorRemoteController::LocalController:
  MemoryLeakReportProto::Params GetParams() const override;
  void SendLeakReports(
      const std::vector<MemoryLeakReportProto>& reports) override;
  void OnRemoteProcessShutdown() override;

 private:
  // Stores a given array of leak reports in |stored_reports_|. |process_type|
  // is the type of process that generated these reports. The reports must all
  // come from the same process type.
  void StoreLeakReports(const std::vector<MemoryLeakReportProto>& reports,
                        MemoryLeakReportProto::ProcessType process_type);

  // All leak reports received through OnLeakFound() are stored in protobuf
  // format.
  std::vector<MemoryLeakReportProto> stored_reports_;

  // Contains all the parameters passed to LeakDetector. Store them in a single
  // protobuf message instead of in separate member variables.
  const MemoryLeakReportProto::Params params_;

  // The build ID of the current Chrome binary.
  std::vector<uint8_t> build_id_;

  // For thread safety.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorController);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_CONTROLLER_H_
