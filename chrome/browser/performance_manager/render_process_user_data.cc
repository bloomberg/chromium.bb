// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/render_process_user_data.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/public/cpp/connector.h"

namespace performance_manager {
namespace {

const void* const kRenderProcessUserDataKey = &kRenderProcessUserDataKey;

}  // namespace

RenderProcessUserData* RenderProcessUserData::first_ = nullptr;

RenderProcessUserData::RenderProcessUserData(
    content::RenderProcessHost* render_process_host)
    : host_(render_process_host),
      process_resource_coordinator_(
          performance_manager::PerformanceManager::GetInstance()) {
  // The process itself shouldn't have been created at this point.
  DCHECK(!host_->GetProcess().IsValid() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess));
  host_->AddObserver(this);

  // Push this instance to the list.
  next_ = first_;
  if (next_)
    next_->prev_ = this;
  prev_ = nullptr;
  first_ = this;
}

RenderProcessUserData::~RenderProcessUserData() {
  host_->RemoveObserver(this);

  if (first_ == this)
    first_ = next_;

  if (prev_) {
    DCHECK_EQ(prev_->next_, this);
    prev_->next_ = next_;
  }
  if (next_) {
    DCHECK_EQ(next_->prev_, this);
    next_->prev_ = prev_;
  }
}

// static
void RenderProcessUserData::DetachAndDestroyAll() {
  while (first_)
    first_->host_->RemoveUserData(kRenderProcessUserDataKey);
}

void RenderProcessUserData::CreateForRenderProcessHost(
    content::RenderProcessHost* host) {
  std::unique_ptr<RenderProcessUserData> user_data =
      base::WrapUnique(new RenderProcessUserData(host));

  host->SetUserData(kRenderProcessUserDataKey, std::move(user_data));
}

RenderProcessUserData* RenderProcessUserData::GetForRenderProcessHost(
    content::RenderProcessHost* host) {
  return static_cast<RenderProcessUserData*>(
      host->GetUserData(kRenderProcessUserDataKey));
}

void RenderProcessUserData::RenderProcessReady(
    content::RenderProcessHost* host) {
  // TODO(siggi): Rename OnProcessLaunched->OnProcessReady.
  process_resource_coordinator_.OnProcessLaunched(host->GetProcess());
}

void RenderProcessUserData::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  process_resource_coordinator_.SetProcessExitStatus(info.exit_code);
}

}  // namespace performance_manager
