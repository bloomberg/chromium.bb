// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"

namespace extensions {

namespace {

// URL schemes for which we'll send events.
const char* kValidSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kJavaScriptScheme,
  chrome::kDataScheme,
  chrome::kFileSystemScheme,
};

}  // namespace

// static
bool FrameNavigationState::allow_extension_scheme_ = false;

FrameNavigationState::FrameNavigationState()
    : main_frame_id_(-1) {
}

FrameNavigationState::~FrameNavigationState() {}

bool FrameNavigationState::CanSendEvents(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end() ||
      frame_state->second.error_occurred) {
    return false;
  }
  return IsValidUrl(frame_state->second.url);
}

bool FrameNavigationState::IsValidUrl(const GURL& url) const {
  for (unsigned i = 0; i < arraysize(kValidSchemes); ++i) {
    if (url.scheme() == kValidSchemes[i])
      return true;
  }
  // Allow about:blank.
  if (url.spec() == chrome::kAboutBlankURL)
    return true;
  if (allow_extension_scheme_ && url.scheme() == chrome::kExtensionScheme)
    return true;
  return false;
}

void FrameNavigationState::TrackFrame(int64 frame_id,
                                      const GURL& url,
                                      bool is_main_frame,
                                      bool is_error_page) {
  if (is_main_frame) {
    frame_state_map_.clear();
    frame_ids_.clear();
  }
  FrameState& frame_state = frame_state_map_[frame_id];
  frame_state.error_occurred = is_error_page;
  frame_state.url = url;
  frame_state.is_main_frame = is_main_frame;
  frame_state.is_navigating = true;
  frame_state.is_committed = false;
  frame_state.is_server_redirected = false;
  if (is_main_frame) {
    main_frame_id_ = frame_id;
  }
  frame_ids_.insert(frame_id);
}

void FrameNavigationState::UpdateFrame(int64 frame_id, const GURL& url) {
  FrameIdToStateMap::iterator frame_state = frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return;
  }
  frame_state->second.url = url;
}

bool FrameNavigationState::IsValidFrame(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end());
}

GURL FrameNavigationState::GetUrl(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return GURL();
  }
  return frame_state->second.url;
}

bool FrameNavigationState::IsMainFrame(int64 frame_id) const {
  return main_frame_id_ != -1 && main_frame_id_ == frame_id;
}

int64 FrameNavigationState::GetMainFrameID() const {
  return main_frame_id_;
}

void FrameNavigationState::SetErrorOccurredInFrame(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].error_occurred = true;
}

bool FrameNavigationState::GetErrorOccurredInFrame(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state == frame_state_map_.end() ||
          frame_state->second.error_occurred);
}

void FrameNavigationState::SetNavigationCompleted(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_navigating = false;
}

bool FrameNavigationState::GetNavigationCompleted(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state == frame_state_map_.end() ||
          !frame_state->second.is_navigating);
}

void FrameNavigationState::SetNavigationCommitted(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_committed = true;
}

bool FrameNavigationState::GetNavigationCommitted(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_committed);
}

void FrameNavigationState::SetIsServerRedirected(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_server_redirected = true;
}

bool FrameNavigationState::GetIsServerRedirected(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_server_redirected);
}

}  // namespace extensions
