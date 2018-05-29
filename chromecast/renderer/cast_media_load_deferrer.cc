// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_media_load_deferrer.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"

namespace chromecast {

CastMediaLoadDeferrer::CastMediaLoadDeferrer(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CastMediaLoadDeferrer>(render_frame),
      render_frame_action_blocked_(false) {
  registry_.AddInterface(
      base::BindRepeating(&CastMediaLoadDeferrer::OnMediaLoadDeferrerRequest,
                          base::Unretained(this)));
}

CastMediaLoadDeferrer::~CastMediaLoadDeferrer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CastMediaLoadDeferrer::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete this;
}

void CastMediaLoadDeferrer::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

bool CastMediaLoadDeferrer::RunWhenInForeground(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!render_frame_action_blocked_) {
    std::move(closure).Run();
    return false;
  }

  LOG(WARNING) << "A render frame action is being blocked.";
  pending_closures_.push_back(std::move(closure));
  return true;
}

// MediaLoadDeferrer implementation
void CastMediaLoadDeferrer::UpdateMediaLoadStatus(bool blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  render_frame_action_blocked_ = blocked;
  if (blocked) {
    LOG(INFO) << "Render frame actions are blocked.";
    return;
  }
  // Move callbacks in case OnBlockMediaLoading() is called somehow
  // during iteration.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(pending_closures_);
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
  LOG(INFO) << "Render frame actions are unblocked.";
}

void CastMediaLoadDeferrer::OnMediaLoadDeferrerRequest(
    chromecast::shell::mojom::MediaLoadDeferrerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace chromecast
