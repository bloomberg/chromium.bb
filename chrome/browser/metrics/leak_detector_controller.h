// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_LEAK_DETECTOR_CONTROLLER_H_
#define CHROME_BROWSER_METRICS_LEAK_DETECTOR_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/leak_detector/leak_detector.h"
#include "components/metrics/proto/memory_leak_report.pb.h"

namespace metrics {

// This class initializes the LeakDetector on the browser process and registers
// itself to be notified of leak reports.
class LeakDetectorController : public LeakDetector::Observer {
 public:
  LeakDetectorController();
  ~LeakDetectorController() override;

  // Retrieves all reports in |stored_reports_|, moving them to |*reports|.
  // |stored_reports_| is empty afterwards.
  void GetLeakReports(std::vector<MemoryLeakReportProto>* reports);

 protected:
  // LeakDetector::Observer:
  void OnLeaksFound(const std::vector<MemoryLeakReportProto>& reports) override;

 private:
  // All leak reports received through OnLeakFound() are stored in protobuf
  // format.
  std::vector<MemoryLeakReportProto> stored_reports_;

  // All incoming leak reports should be converted to a MemoryLeakReportProto
  // starting with this template, which has some fields already filled in, e.g.
  // leak detection parameters. Storing the LeakDetector parameters in the
  // protobuf template avoids having to store these params as individual member
  // variables.
  const MemoryLeakReportProto leak_report_proto_template_;

  // For thread safety.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorController);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_LEAK_DETECTOR_CONTROLLER_H_
