// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// URL schemes for which we'll send events.
const char* kValidSchemes[] = {
  chrome::kChromeUIScheme,
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kJavaScriptScheme,
  chrome::kDataScheme,
  chrome::kFileSystemScheme,
};

}  // namespace

FrameNavigationState::FrameID::FrameID()
    : frame_num(-1),
      render_view_host(NULL) {
}

FrameNavigationState::FrameID::FrameID(
    int64 frame_num,
    content::RenderViewHost* render_view_host)
    : frame_num(frame_num),
      render_view_host(render_view_host) {
}

bool FrameNavigationState::FrameID::operator<(
    const FrameNavigationState::FrameID& other) const {
  return frame_num < other.frame_num ||
      (frame_num == other.frame_num &&
       render_view_host < other.render_view_host);
}

bool FrameNavigationState::FrameID::operator==(
    const FrameNavigationState::FrameID& other) const {
  return frame_num == other.frame_num &&
      render_view_host == other.render_view_host;
}

bool FrameNavigationState::FrameID::operator!=(
    const FrameNavigationState::FrameID& other) const {
  return !(*this == other);
}

FrameNavigationState::FrameState::FrameState() {}

// static
bool FrameNavigationState::allow_extension_scheme_ = false;

FrameNavigationState::FrameNavigationState() {}

FrameNavigationState::~FrameNavigationState() {}

bool FrameNavigationState::CanSendEvents(FrameID frame_id) const {
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
  // Allow about:blank and about:srcdoc.
  if (url.spec() == chrome::kAboutBlankURL ||
      url.spec() == chrome::kAboutSrcDocURL) {
    return true;
  }
  if (allow_extension_scheme_ && url.scheme() == extensions::kExtensionScheme)
    return true;
  return false;
}

void FrameNavigationState::TrackFrame(FrameID frame_id,
                                      FrameID parent_frame_id,
                                      const GURL& url,
                                      bool is_main_frame,
                                      bool is_error_page,
                                      bool is_iframe_srcdoc) {
  FrameState& frame_state = frame_state_map_[frame_id];
  frame_state.error_occurred = is_error_page;
  frame_state.url = url;
  frame_state.is_main_frame = is_main_frame;
  frame_state.is_iframe_srcdoc = is_iframe_srcdoc;
  DCHECK(!is_iframe_srcdoc || url == GURL(chrome::kAboutBlankURL));
  frame_state.is_navigating = true;
  frame_state.is_committed = false;
  frame_state.is_server_redirected = false;
  if (!is_main_frame) {
    frame_state.parent_frame_num = parent_frame_id.frame_num;
  } else {
    DCHECK(parent_frame_id.frame_num == -1);
    frame_state.parent_frame_num = -1;
  }
  frame_ids_.insert(frame_id);
}

void FrameNavigationState::FrameDetached(FrameID frame_id) {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return;
  }
  if (frame_id == main_frame_id_)
    main_frame_id_ = FrameID();
  frame_state_map_.erase(frame_id);
  frame_ids_.erase(frame_id);
#ifndef NDEBUG
  // Check that the deleted frame was not the parent of any other frame. WebKit
  // should always detach frames starting with the children.
  for (FrameIdToStateMap::const_iterator frame = frame_state_map_.begin();
       frame != frame_state_map_.end(); ++frame) {
    if (frame->first.render_view_host != frame_id.render_view_host)
      continue;
    if (frame->second.parent_frame_num != frame_id.frame_num)
      continue;
    NOTREACHED();
  }
#endif
}

void FrameNavigationState::StopTrackingFramesInRVH(
    content::RenderViewHost* render_view_host,
    FrameID id_to_skip) {
  for (std::set<FrameID>::iterator frame = frame_ids_.begin();
       frame != frame_ids_.end();) {
    if (frame->render_view_host != render_view_host || *frame == id_to_skip) {
      ++frame;
      continue;
    }
    FrameID frame_id = *frame;
    ++frame;
    if (frame_id == main_frame_id_)
      main_frame_id_ = FrameID();
    frame_state_map_.erase(frame_id);
    frame_ids_.erase(frame_id);
  }
}

void FrameNavigationState::UpdateFrame(FrameID frame_id, const GURL& url) {
  FrameIdToStateMap::iterator frame_state = frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return;
  }
  frame_state->second.url = url;
}

bool FrameNavigationState::IsValidFrame(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end());
}

GURL FrameNavigationState::GetUrl(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return GURL();
  }
  if (frame_state->second.is_iframe_srcdoc)
    return GURL(chrome::kAboutSrcDocURL);
  return frame_state->second.url;
}

bool FrameNavigationState::IsMainFrame(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_main_frame);
}

FrameNavigationState::FrameID FrameNavigationState::GetMainFrameID() const {
  return main_frame_id_;
}

FrameNavigationState::FrameID FrameNavigationState::GetParentFrameID(
    FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return FrameID();
  }
  return FrameID(frame_state->second.parent_frame_num,
                 frame_id.render_view_host);
}

void FrameNavigationState::SetErrorOccurredInFrame(FrameID frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].error_occurred = true;
}

bool FrameNavigationState::GetErrorOccurredInFrame(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state == frame_state_map_.end() ||
          frame_state->second.error_occurred);
}

void FrameNavigationState::SetNavigationCompleted(FrameID frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_navigating = false;
}

bool FrameNavigationState::GetNavigationCompleted(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state == frame_state_map_.end() ||
          !frame_state->second.is_navigating);
}

void FrameNavigationState::SetNavigationCommitted(FrameID frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_committed = true;
  if (frame_state_map_[frame_id].is_main_frame)
    main_frame_id_ = frame_id;
}

bool FrameNavigationState::GetNavigationCommitted(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_committed);
}

void FrameNavigationState::SetIsServerRedirected(FrameID frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_server_redirected = true;
}

bool FrameNavigationState::GetIsServerRedirected(FrameID frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_server_redirected);
}

}  // namespace extensions
