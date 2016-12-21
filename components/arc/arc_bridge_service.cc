// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_session_runner.h"

namespace arc {

namespace {

const base::Feature kArcEnabledFeature{"EnableARC",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

ArcBridgeService::ArcBridgeService() = default;

ArcBridgeService::~ArcBridgeService() = default;

// static
bool ArcBridgeService::GetEnabled(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kEnableArc) ||
         (command_line->HasSwitch(chromeos::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kArcEnabledFeature));
}

// static
bool ArcBridgeService::GetAvailable(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kArcAvailable);
}

void ArcBridgeService::InitializeArcSessionRunner(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  DCHECK(!arc_session_runner_);
  arc_session_runner_ = std::move(arc_session_runner);
}

void ArcBridgeService::AddObserver(ArcSessionObserver* observer) {
  DCHECK(arc_session_runner_);
  arc_session_runner_->AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(ArcSessionObserver* observer) {
  DCHECK(arc_session_runner_);
  arc_session_runner_->RemoveObserver(observer);
}

void ArcBridgeService::RequestStart() {
  DCHECK(arc_session_runner_);
  arc_session_runner_->RequestStart();
}

void ArcBridgeService::RequestStop() {
  DCHECK(arc_session_runner_);
  arc_session_runner_->RequestStop();
}

void ArcBridgeService::OnShutdown() {
  DCHECK(arc_session_runner_);
  arc_session_runner_->OnShutdown();
}

bool ArcBridgeService::ready() const {
  DCHECK(arc_session_runner_);
  return arc_session_runner_->IsRunning();
}

bool ArcBridgeService::stopped() const {
  DCHECK(arc_session_runner_);
  return arc_session_runner_->IsStopped();
}

}  // namespace arc
