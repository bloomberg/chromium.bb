// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// URL schemes for which we'll send events.
const char* kValidSchemes[] = {
    content::kChromeUIScheme,
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFileScheme,
    url::kFtpScheme,
    url::kJavaScriptScheme,
    url::kDataScheme,
    url::kFileSystemScheme,
};

}  // namespace

FrameNavigationState::FrameState::FrameState() {}

// static
bool FrameNavigationState::allow_extension_scheme_ = false;

FrameNavigationState::FrameNavigationState() : main_frame_host_(NULL) {
}

FrameNavigationState::~FrameNavigationState() {}

bool FrameNavigationState::CanSendEvents(
    content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end() || it->second.error_occurred) {
    return false;
  }
  return IsValidUrl(it->second.url);
}

bool FrameNavigationState::IsValidUrl(const GURL& url) const {
  for (unsigned i = 0; i < arraysize(kValidSchemes); ++i) {
    if (url.scheme() == kValidSchemes[i])
      return true;
  }
  // Allow about:blank and about:srcdoc.
  if (url.spec() == url::kAboutBlankURL ||
      url.spec() == content::kAboutSrcDocURL) {
    return true;
  }
  return allow_extension_scheme_ && url.scheme() == kExtensionScheme;
}

void FrameNavigationState::TrackFrame(content::RenderFrameHost* frame_host,
                                      const GURL& url,
                                      bool is_error_page,
                                      bool is_iframe_srcdoc) {
  FrameState& frame_state = frame_host_state_map_[frame_host];
  frame_state.error_occurred = is_error_page;
  frame_state.url = url;
  frame_state.is_iframe_srcdoc = is_iframe_srcdoc;
  DCHECK(!is_iframe_srcdoc || url == GURL(url::kAboutBlankURL));
  frame_state.is_navigating = true;
  frame_state.is_committed = false;
  frame_state.is_server_redirected = false;
  frame_state.is_parsing = true;
  frame_hosts_.insert(frame_host);
}

void FrameNavigationState::FrameDetached(content::RenderFrameHost* frame_host) {
  if (frame_host == main_frame_host_)
    main_frame_host_ = NULL;
  frame_host_state_map_.erase(frame_host);
  frame_hosts_.erase(frame_host);
}

void FrameNavigationState::StopTrackingFramesInRVH(
    content::RenderViewHost* render_view_host,
    content::RenderFrameHost* frame_host_to_skip) {
  for (std::set<content::RenderFrameHost*>::iterator it = frame_hosts_.begin();
       it != frame_hosts_.end();) {
    if ((*it)->GetRenderViewHost() != render_view_host ||
        *it == frame_host_to_skip) {
      ++it;
      continue;
    }
    if (*it == main_frame_host_)
      main_frame_host_ = NULL;
    frame_host_state_map_.erase(*it);
    frame_hosts_.erase(it++);
  }
}

void FrameNavigationState::UpdateFrame(content::RenderFrameHost* frame_host,
                                       const GURL& url) {
  FrameHostToStateMap::iterator it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.url = url;
}

bool FrameNavigationState::IsValidFrame(
    content::RenderFrameHost* frame_host) const {
  return frame_host_state_map_.find(frame_host) != frame_host_state_map_.end();
}

GURL FrameNavigationState::GetUrl(content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return GURL();
  }
  if (it->second.is_iframe_srcdoc)
    return GURL(content::kAboutSrcDocURL);
  return it->second.url;
}

content::RenderFrameHost* FrameNavigationState::GetLastCommittedMainFrameHost()
    const {
  return main_frame_host_;
}

void FrameNavigationState::SetErrorOccurredInFrame(
    content::RenderFrameHost* frame_host) {
  FrameHostToStateMap::iterator it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.error_occurred = true;
}

bool FrameNavigationState::GetErrorOccurredInFrame(
    content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it == frame_host_state_map_.end() || it->second.error_occurred;
}

void FrameNavigationState::SetNavigationCompleted(
    content::RenderFrameHost* frame_host) {
  FrameHostToStateMap::iterator it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.is_navigating = false;
}

bool FrameNavigationState::GetNavigationCompleted(
    content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it == frame_host_state_map_.end() || !it->second.is_navigating;
}

void FrameNavigationState::SetParsingFinished(
    content::RenderFrameHost* frame_host) {
  FrameHostToStateMap::iterator it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.is_parsing = false;
}

bool FrameNavigationState::GetParsingFinished(
    content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it == frame_host_state_map_.end() || !it->second.is_parsing;
}

void FrameNavigationState::SetNavigationCommitted(
    content::RenderFrameHost* frame_host) {
  FrameHostToStateMap::iterator it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.is_committed = true;
  if (!frame_host->GetParent())
    main_frame_host_ = frame_host;
}

bool FrameNavigationState::GetNavigationCommitted(
    content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it != frame_host_state_map_.end() && it->second.is_committed;
}

void FrameNavigationState::SetIsServerRedirected(
    content::RenderFrameHost* frame_host) {
  FrameHostToStateMap::iterator it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.is_server_redirected = true;
}

bool FrameNavigationState::GetIsServerRedirected(
    content::RenderFrameHost* frame_host) const {
  FrameHostToStateMap::const_iterator it =
      frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it != frame_host_state_map_.end() && it->second.is_server_redirected;
}

}  // namespace extensions
