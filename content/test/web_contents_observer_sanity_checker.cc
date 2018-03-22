// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_contents_observer_sanity_checker.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

const char kWebContentsObserverSanityCheckerKey[] =
    "WebContentsObserverSanityChecker";

GlobalRoutingID GetRoutingPair(RenderFrameHost* host) {
  if (!host)
    return GlobalRoutingID(0, 0);
  return GlobalRoutingID(host->GetProcess()->GetID(), host->GetRoutingID());
}

}  // namespace

// static
void WebContentsObserverSanityChecker::Enable(WebContents* web_contents) {
  if (web_contents->GetUserData(&kWebContentsObserverSanityCheckerKey))
    return;
  web_contents->SetUserData(
      &kWebContentsObserverSanityCheckerKey,
      base::WrapUnique(new WebContentsObserverSanityChecker(web_contents)));
}

void WebContentsObserverSanityChecker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  bool frame_exists = !live_routes_.insert(routing_pair).second;
  deleted_routes_.erase(routing_pair);

  if (frame_exists) {
    CHECK(false) << "RenderFrameCreated called more than once for routing pair:"
                 << Format(render_frame_host);
  }

  CHECK(render_frame_host->GetProcess()->HasConnection())
      << "RenderFrameCreated was called for a RenderFrameHost whose render "
         "process is not currently live, so there's no way for the RenderFrame "
         "to have been created.";
  CHECK(
      static_cast<RenderFrameHostImpl*>(render_frame_host)->IsRenderFrameLive())
      << "RenderFrameCreated called on for a RenderFrameHost that thinks it is "
         "not alive.";

  EnsureStableParentValue(render_frame_host);
  CHECK(!HasAnyChildren(render_frame_host));
  if (render_frame_host->GetParent()) {
    // It should also be a current host.
    GlobalRoutingID parent_routing_pair =
        GetRoutingPair(render_frame_host->GetParent());

    CHECK(current_hosts_.count(parent_routing_pair))
        << "RenderFrameCreated called for a RenderFrameHost whose parent was "
        << "not a current RenderFrameHost. Only the current frame should be "
        << "spawning children.";
  }
}

void WebContentsObserverSanityChecker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  bool was_live = !!live_routes_.erase(routing_pair);
  bool was_dead_already = !deleted_routes_.insert(routing_pair).second;

  if (was_dead_already) {
    CHECK(false) << "RenderFrameDeleted called more than once for routing pair "
                 << Format(render_frame_host);
  } else if (!was_live) {
    CHECK(false) << "RenderFrameDeleted called for routing pair "
                 << Format(render_frame_host)
                 << " for which RenderFrameCreated was never called";
  }

  EnsureStableParentValue(render_frame_host);
  CHECK(!HasAnyChildren(render_frame_host));
  if (render_frame_host->GetParent())
    AssertRenderFrameExists(render_frame_host->GetParent());

  // All players should have been paused by this point.
  for (const auto& id : active_media_players_)
    CHECK_NE(id.first, render_frame_host);
}

void WebContentsObserverSanityChecker::RenderFrameForInterstitialPageCreated(
    RenderFrameHost* render_frame_host) {
  // TODO(nick): Record this.
}

void WebContentsObserverSanityChecker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CHECK(new_host);
  CHECK_NE(new_host, old_host);

  if (old_host) {
    EnsureStableParentValue(old_host);
    CHECK_EQ(old_host->GetParent(), new_host->GetParent());
    GlobalRoutingID routing_pair = GetRoutingPair(old_host);
    bool old_did_exist = !!current_hosts_.erase(routing_pair);
    if (!old_did_exist) {
      CHECK(false)
          << "RenderFrameHostChanged called with old host that did not exist:"
          << Format(old_host);
    }
    CHECK(!HasAnyChildren(old_host))
        << "All children should be detached before a parent is detached.";
  }

  EnsureStableParentValue(new_host);
  if (new_host->GetParent()) {
    AssertRenderFrameExists(new_host->GetParent());
    CHECK(current_hosts_.count(GetRoutingPair(new_host->GetParent())))
        << "Parent of frame being committed must be current.";
  }

  GlobalRoutingID routing_pair = GetRoutingPair(new_host);
  bool host_exists = !current_hosts_.insert(routing_pair).second;
  if (host_exists) {
    CHECK(false)
        << "RenderFrameHostChanged called more than once for routing pair:"
        << Format(new_host);
  }
  CHECK(!HasAnyChildren(new_host))
      << "A frame should not have children before it is committed.";
}

void WebContentsObserverSanityChecker::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  // A frame can be deleted before RenderFrame in the renderer process is
  // created, so there is not much that can be enforced here.
  CHECK(!web_contents_destroyed_);

  EnsureStableParentValue(render_frame_host);

  CHECK(!HasAnyChildren(render_frame_host))
      << "All children should be deleted before a frame is detached.";

  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  CHECK(current_hosts_.erase(routing_pair))
      << "FrameDeleted called with a non-current RenderFrameHost.";

  if (render_frame_host->GetParent())
    AssertRenderFrameExists(render_frame_host->GetParent());
}

void WebContentsObserverSanityChecker::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(!NavigationIsOngoing(navigation_handle));

  CHECK(navigation_handle->GetNetErrorCode() == net::OK);
  CHECK(!navigation_handle->HasCommitted());
  CHECK(!navigation_handle->IsErrorPage());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());

  ongoing_navigations_.insert(navigation_handle);
}

void WebContentsObserverSanityChecker::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  CHECK(navigation_handle->GetNetErrorCode() == net::OK);
  CHECK(!navigation_handle->HasCommitted());
  CHECK(!navigation_handle->IsErrorPage());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
}

void WebContentsObserverSanityChecker::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  CHECK(!navigation_handle->HasCommitted());
  CHECK(navigation_handle->GetRenderFrameHost());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
  CHECK(navigation_handle->GetRenderFrameHost() != nullptr);
}

void WebContentsObserverSanityChecker::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  CHECK(!(navigation_handle->HasCommitted() &&
          !navigation_handle->IsErrorPage()) ||
        navigation_handle->GetNetErrorCode() == net::OK);
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());

  CHECK(!navigation_handle->HasCommitted() ||
        navigation_handle->GetRenderFrameHost() != nullptr);

  ongoing_navigations_.erase(navigation_handle);
}

void WebContentsObserverSanityChecker::DocumentAvailableInMainFrame() {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DocumentOnLoadCompletedInMainFrame() {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DocumentLoadedInFrame(
    RenderFrameHost* render_frame_host) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFinishLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  AssertRenderFrameExists(source_render_frame_host);
}

void WebContentsObserverSanityChecker::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  CHECK(!web_contents_destroyed_);
  CHECK(std::find(active_media_players_.begin(), active_media_players_.end(),
                  id) == active_media_players_.end());
  active_media_players_.push_back(id);
}

void WebContentsObserverSanityChecker::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  CHECK(!web_contents_destroyed_);
  CHECK(std::find(active_media_players_.begin(), active_media_players_.end(),
                  id) != active_media_players_.end());
  active_media_players_.erase(std::remove(active_media_players_.begin(),
                                          active_media_players_.end(), id),
                              active_media_players_.end());
}

bool WebContentsObserverSanityChecker::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  // FrameHostMsg_RenderProcessGone is special internal IPC message that
  // should not be leaking outside of RenderFrameHost.
  CHECK(message.type() != FrameHostMsg_RenderProcessGone::ID);
  CHECK(render_frame_host->IsRenderFrameLive());

  AssertRenderFrameExists(render_frame_host);
  return false;
}

void WebContentsObserverSanityChecker::WebContentsDestroyed() {
  CHECK(!web_contents_destroyed_);
  web_contents_destroyed_ = true;
  CHECK(ongoing_navigations_.empty());
  CHECK(active_media_players_.empty());
  CHECK(live_routes_.empty());
}

void WebContentsObserverSanityChecker::DidStartLoading() {
  // TODO(clamy): add checks for the loading state in the rest of observer
  // methods.
  CHECK(!is_loading_);
  CHECK(web_contents()->IsLoading());
  is_loading_ = true;
}

void WebContentsObserverSanityChecker::DidStopLoading() {
  CHECK(is_loading_);
  CHECK(!web_contents()->IsLoading());
  is_loading_ = false;
}

WebContentsObserverSanityChecker::WebContentsObserverSanityChecker(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_loading_(false),
      web_contents_destroyed_(false) {}

WebContentsObserverSanityChecker::~WebContentsObserverSanityChecker() {
  CHECK(web_contents_destroyed_);
}

void WebContentsObserverSanityChecker::AssertRenderFrameExists(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);

  bool render_frame_created_happened = live_routes_.count(routing_pair) != 0;
  bool render_frame_deleted_happened = deleted_routes_.count(routing_pair) != 0;

  CHECK(render_frame_created_happened)
      << "A RenderFrameHost pointer was passed to a WebContentsObserver "
      << "method, but WebContentsObserver::RenderFrameCreated was never called "
      << "for that RenderFrameHost: " << Format(render_frame_host);
  CHECK(!render_frame_deleted_happened)
      << "A RenderFrameHost pointer was passed to a WebContentsObserver "
      << "method, but WebContentsObserver::RenderFrameDeleted had already been "
      << "called on that frame:" << Format(render_frame_host);
}

void WebContentsObserverSanityChecker::AssertMainFrameExists() {
  AssertRenderFrameExists(web_contents()->GetMainFrame());
}

std::string WebContentsObserverSanityChecker::Format(
    RenderFrameHost* render_frame_host) {
  return base::StringPrintf(
      "(%d, %d -> %s)", render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(),
      render_frame_host->GetSiteInstance()->GetSiteURL().spec().c_str());
}

bool WebContentsObserverSanityChecker::NavigationIsOngoing(
    NavigationHandle* navigation_handle) {
  auto it = ongoing_navigations_.find(navigation_handle);
  return it != ongoing_navigations_.end();
}

void WebContentsObserverSanityChecker::EnsureStableParentValue(
    RenderFrameHost* render_frame_host) {
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  GlobalRoutingID parent_routing_pair =
      GetRoutingPair(render_frame_host->GetParent());

  auto it = parent_ids_.find(routing_pair);
  if (it == parent_ids_.end()) {
    parent_ids_.insert(std::make_pair(routing_pair, parent_routing_pair));
  } else {
    GlobalRoutingID former_parent_routing_pair = it->second;
    CHECK(former_parent_routing_pair == parent_routing_pair)
        << "RFH's parent value changed over time! That is really not good!";
  }
}

bool WebContentsObserverSanityChecker::HasAnyChildren(RenderFrameHost* parent) {
  GlobalRoutingID parent_routing_pair = GetRoutingPair(parent);
  for (auto& entry : parent_ids_) {
    if (entry.second == parent_routing_pair) {
      if (live_routes_.count(entry.first))
        return true;
      if (current_hosts_.count(entry.first))
        return true;
    }
  }
  return false;
}

}  // namespace content
