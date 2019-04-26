// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporting_controller.h"

#include "cc/scheduler/compositor_frame_reporter.h"

namespace cc {
CompositorFrameReportingController::CompositorFrameReportingController() {}

CompositorFrameReportingController::~CompositorFrameReportingController() {
  for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
    if (reporters_[i]) {
      reporters_[i]->SetFrameTerminationStatus(
          CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame);
    }
  }
}

void CompositorFrameReportingController::WillBeginImplFrame() {
  std::unique_ptr<CompositorFrameReporter> reporter =
      std::make_unique<CompositorFrameReporter>();
  reporter->StartStage("BeginImplFrameToSendBeginMainFrame");
  if (reporters_[PipelineStage::kBeginImplFrame])
    reporters_[PipelineStage::kBeginImplFrame]->SetFrameTerminationStatus(
        CompositorFrameReporter::FrameTerminationStatus::
            kReplacedByNewReporter);
  reporters_[PipelineStage::kBeginImplFrame] = std::move(reporter);
}

void CompositorFrameReportingController::WillBeginMainFrame() {
  DCHECK(reporters_[PipelineStage::kBeginImplFrame]);
  DCHECK(reporters_[PipelineStage::kBeginMainFrame] !=
         reporters_[PipelineStage::kBeginImplFrame]);
  reporters_[PipelineStage::kBeginImplFrame]->StartStage(
      "SendBeginMainFrameToCommit");
  AdvanceReporterStage(PipelineStage::kBeginImplFrame,
                       PipelineStage::kBeginMainFrame);
}

void CompositorFrameReportingController::BeginMainFrameAborted() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  std::unique_ptr<CompositorFrameReporter> aborted_frame_reporter =
      std::move(reporters_[PipelineStage::kBeginMainFrame]);
  aborted_frame_reporter->StartStage("BeginMainFrameAborted");
  aborted_frame_reporter->SetFrameTerminationStatus(
      CompositorFrameReporter::FrameTerminationStatus::kMainFrameAborted);
}

void CompositorFrameReportingController::WillCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage("Commit");
}

void CompositorFrameReportingController::DidCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(
      "EndCommitToActivation");
  AdvanceReporterStage(PipelineStage::kBeginMainFrame, PipelineStage::kCommit);
}

void CompositorFrameReportingController::WillInvalidateOnImplSide() {
  // Allows for activation without committing.
  // TODO(alsan): Report latency of impl side invalidations.
  next_activate_has_invalidation_ = true;
}
void CompositorFrameReportingController::WillActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage("Activation");
}

void CompositorFrameReportingController::DidActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  next_activate_has_invalidation_ = false;
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(
      "EndActivateToSubmitCompositorFrame");
  AdvanceReporterStage(PipelineStage::kCommit, PipelineStage::kActivate);
}

void CompositorFrameReportingController::DidSubmitCompositorFrame() {
  if (!reporters_[PipelineStage::kActivate])
    return;
  std::unique_ptr<CompositorFrameReporter> submitted_reporter =
      std::move(reporters_[PipelineStage::kActivate]);
  submitted_reporter->StartStage("SubmitCompositorFrame");
  // If there are any other reporters active on the other stages of the
  // pipeline then that means a new frame was started during the duration of
  // this reporter and therefore the frame being tracked missed the deadline.
  if (reporters_[PipelineStage::kBeginImplFrame] ||
      reporters_[PipelineStage::kBeginMainFrame] ||
      reporters_[PipelineStage::kCommit]) {
    submitted_reporter->SetFrameTerminationStatus(
        CompositorFrameReporter::FrameTerminationStatus::
            kSubmittedFrameMissedDeadline);
  } else {
    submitted_reporter->SetFrameTerminationStatus(
        CompositorFrameReporter::FrameTerminationStatus::kSubmittedFrame);
  }
}

void CompositorFrameReportingController::DidNotProduceFrame() {
  if (reporters_[PipelineStage::kActivate])
    reporters_[PipelineStage::kActivate]->SetFrameTerminationStatus(
        CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame);
  reporters_[PipelineStage::kActivate] = nullptr;
}

void CompositorFrameReportingController::AdvanceReporterStage(
    PipelineStage start,
    PipelineStage target) {
  if (reporters_[target]) {
    reporters_[target]->SetFrameTerminationStatus(
        CompositorFrameReporter::FrameTerminationStatus::
            kReplacedByNewReporter);
  }
  reporters_[target] = std::move(reporters_[start]);
}
}  // namespace cc
