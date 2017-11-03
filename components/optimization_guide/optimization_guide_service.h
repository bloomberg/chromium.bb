// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/version.h"

namespace optimization_guide {

// Encapsulates information about a version of optimization hints data received
// from the components server.
struct UnindexedHintsInfo {
  UnindexedHintsInfo(const base::Version& hints_version,
                     const base::FilePath& hints_path);
  ~UnindexedHintsInfo();

  // The version of the hints content.
  const base::Version hints_version;

  // The path to the file containing the unindexed hints.
  const base::FilePath hints_path;
};

// Reads and indexes the hints downloaded from the Component Updater as part of
// the Optimization Hints component.
class OptimizationGuideService {
 public:
  OptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);

  virtual ~OptimizationGuideService();

  // Reads and indexes hints from the given unindexed hints, unless its
  // |hints_version| matches that of the most recently parsed version, in which
  // case it does nothing.
  //
  // Virtual so it can be mocked out in tests.
  virtual void ReadAndIndexHints(
      const UnindexedHintsInfo& unindexed_hints_info);

 private:
  // Always called as part of a background priority task.
  void ReadAndIndexHintsInBackground(
      const UnindexedHintsInfo& unindexed_hints_info);

  // Runner for indexing tasks.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Runner for IO Thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideService);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
