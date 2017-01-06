// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"

#include "base/lazy_instance.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SingleTabModeTabHelper);

namespace {

typedef std::pair<int32_t, int32_t> RenderFrameHostID;
typedef std::set<RenderFrameHostID> SingleTabIDSet;
base::LazyInstance<SingleTabIDSet> g_blocked_ids = LAZY_INSTANCE_INITIALIZER;

void AddPairOnIOThread(int32_t process_id, int32_t frame_routing_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  RenderFrameHostID single_tab_pair(process_id, frame_routing_id);
  g_blocked_ids.Get().insert(single_tab_pair);
}

void RemovePairOnIOThread(int32_t process_id, int32_t frame_routing_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  RenderFrameHostID single_tab_pair(process_id, frame_routing_id);
  SingleTabIDSet::iterator itr = g_blocked_ids.Get().find(single_tab_pair);
  DCHECK(itr != g_blocked_ids.Get().end());
  g_blocked_ids.Get().erase(itr);
}

void AddPair(content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  int32_t process_id = render_frame_host->GetProcess()->GetID();
  int32_t frame_routing_id = render_frame_host->GetRoutingID();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&AddPairOnIOThread, process_id, frame_routing_id));
}

void RemovePair(content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  int32_t process_id = render_frame_host->GetProcess()->GetID();
  int32_t frame_routing_id = render_frame_host->GetRoutingID();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RemovePairOnIOThread, process_id, frame_routing_id));
}

}  // namespace

SingleTabModeTabHelper::SingleTabModeTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      block_all_new_windows_(false) {
}

SingleTabModeTabHelper::~SingleTabModeTabHelper() {
}

void SingleTabModeTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!block_all_new_windows_)
    return;
  AddPair(render_frame_host);
}

void SingleTabModeTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!block_all_new_windows_)
    return;
  RemovePair(render_frame_host);
}

void SingleTabModeTabHelper::PermanentlyBlockAllNewWindows() {
  block_all_new_windows_ = true;
  for (content::RenderFrameHost* frame : web_contents()->GetAllFrames())
    AddPair(frame);
}

bool SingleTabModeTabHelper::IsRegistered(int32_t process_id,
                                          int32_t frame_routing_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  RenderFrameHostID single_tab_pair(process_id, frame_routing_id);
  SingleTabIDSet::iterator itr = g_blocked_ids.Get().find(single_tab_pair);
  return itr != g_blocked_ids.Get().end();
}

void SingleTabModeTabHelper::HandleOpenUrl(const BlockedWindowParams& params) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  if (!tab)
    return;

  chrome::NavigateParams nav_params =
      params.CreateNavigateParams(web_contents());
  tab->HandlePopupNavigation(&nav_params);
}
