// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_contents_observer_sanity_checker.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

namespace {

const char kWebContentsObserverSanityCheckerKey[] =
    "WebContentsObserverSanityChecker";

}  // namespace

// static
void WebContentsObserverSanityChecker::Enable(WebContents* web_contents) {
  if (web_contents->GetUserData(&kWebContentsObserverSanityCheckerKey))
    return;
  web_contents->SetUserData(&kWebContentsObserverSanityCheckerKey,
                            new WebContentsObserverSanityChecker(web_contents));
}

void WebContentsObserverSanityChecker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  std::pair<int, int> routing_pair =
      std::make_pair(render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID());
  bool frame_exists = !live_routes_.insert(routing_pair).second;
  bool dead_already = deleted_routes_.count(routing_pair) != 0;

  if (frame_exists) {
// TODO(nick): Disabled because of http://crbug.com/425397
#if 0
    CHECK(false) << "RenderFrameCreated called more than once for routing pair:"
                 << Format(render_frame_host);
#endif
  } else if (dead_already) {
    CHECK(false) << "RenderFrameCreated called for routing pair that was "
                 << "previously deleted: " << Format(render_frame_host);
  }
}

void WebContentsObserverSanityChecker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  std::pair<int, int> routing_pair =
      std::make_pair(render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID());
  bool was_live = !!live_routes_.erase(routing_pair);
  bool was_dead_already = !deleted_routes_.insert(routing_pair).second;

  if (was_dead_already) {
    CHECK(false) << "RenderFrameDeleted called more than once for routing pair "
                 << Format(render_frame_host);
  } else if (!was_live) {
// TODO(nick): Clients can easily ignore an unrecognized object, but it
// would be useful from a finding-bugs perspective if we could enable this
// check.
#if 0
    CHECK(false) << "RenderFrameDeleted called for routing pair "
                 << Format(render_frame_host)
                 << " for which RenderFrameCreated was never called";
#endif
  }
}

void WebContentsObserverSanityChecker::RenderFrameForInterstitialPageCreated(
    RenderFrameHost* render_frame_host) {
  // TODO(nick): Record this.
}

void WebContentsObserverSanityChecker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CHECK(new_host);
  if (old_host)
    AssertFrameExists(old_host);
  AssertFrameExists(new_host);
}

void WebContentsObserverSanityChecker::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidStartProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFailProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DidNavigateAnyFrame(
    RenderFrameHost* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DocumentAvailableInMainFrame() {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DocumentOnLoadCompletedInMainFrame() {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DocumentLoadedInFrame(
    RenderFrameHost* render_frame_host) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFinishLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidGetRedirectForResourceRequest(
    RenderFrameHost* render_frame_host,
    const ResourceRedirectDetails& details) {
  AssertFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition) {
  AssertFrameExists(source_render_frame_host);
}

bool WebContentsObserverSanityChecker::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
#if !defined(OS_MACOSX)
// TODO(avi): Disabled because of http://crbug.com/445054
  AssertFrameExists(render_frame_host);
#endif
  return false;
}

void WebContentsObserverSanityChecker::WebContentsDestroyed() {
  CHECK(!web_contents_destroyed_);
  web_contents_destroyed_ = true;
}

WebContentsObserverSanityChecker::WebContentsObserverSanityChecker(
    WebContents* web_contents)
    : WebContentsObserver(web_contents), web_contents_destroyed_(false) {
  // Prime the pump with the initial objects.
  RenderViewCreated(web_contents->GetRenderViewHost());
  RenderFrameCreated(web_contents->GetMainFrame());
}

WebContentsObserverSanityChecker::~WebContentsObserverSanityChecker() {
  CHECK(web_contents_destroyed_);
}

void WebContentsObserverSanityChecker::AssertFrameExists(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  std::pair<int, int> routing_pair =
      std::make_pair(render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID());
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
  AssertFrameExists(web_contents()->GetMainFrame());
}

std::string WebContentsObserverSanityChecker::Format(
    RenderFrameHost* render_frame_host) {
  return base::StringPrintf(
      "(%d, %d -> %s )", render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(),
      render_frame_host->GetSiteInstance()->GetSiteURL().spec().c_str());
}

}  // namespace content
