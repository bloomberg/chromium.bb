// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_message_filter.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

void OnChannelClosingInUIThread(Profile* profile, int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;
  prerender::PrerenderLinkManager* prerender_link_manager =
      prerender::PrerenderLinkManagerFactory::GetForProfile(profile);
  if (!prerender_link_manager)
    return;
  prerender_link_manager->OnChannelClosing(render_process_id);
}

}  // namespace

namespace prerender {

PrerenderMessageFilter::PrerenderMessageFilter(int render_process_id,
                                               Profile* profile)
    : BrowserMessageFilter(PrerenderMsgStart),
      render_process_id_(render_process_id),
      profile_(profile) {
}

PrerenderMessageFilter::~PrerenderMessageFilter() {
}

bool PrerenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(PrerenderHostMsg_AddLinkRelPrerender, OnAddPrerender)
    IPC_MESSAGE_HANDLER(
        PrerenderHostMsg_CancelLinkRelPrerender, OnCancelPrerender)
    IPC_MESSAGE_HANDLER(
        PrerenderHostMsg_AbandonLinkRelPrerender, OnAbandonPrerender)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PrerenderHostMsg_AddLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_CancelLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_AbandonLinkRelPrerender::ID) {
    *thread = BrowserThread::UI;
  }
}

void PrerenderMessageFilter::OnChannelClosing() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnChannelClosingInUIThread, profile_, render_process_id_));
}

void PrerenderMessageFilter::OnAddPrerender(
    int prerender_id,
    const PrerenderAttributes& attributes,
    const content::Referrer& referrer,
    const gfx::Size& size,
    int render_view_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderLinkManager* prerender_link_manager =
      PrerenderLinkManagerFactory::GetForProfile(profile_);
  if (!prerender_link_manager)
    return;
  prerender_link_manager->OnAddPrerender(
      render_process_id_, prerender_id,
      attributes.url, attributes.rel_types, referrer,
      size, render_view_route_id);
}

void PrerenderMessageFilter::OnCancelPrerender(
    int prerender_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderLinkManager* prerender_link_manager =
      PrerenderLinkManagerFactory::GetForProfile(profile_);
  if (!prerender_link_manager)
    return;
  prerender_link_manager->OnCancelPrerender(render_process_id_, prerender_id);
}

void PrerenderMessageFilter::OnAbandonPrerender(
    int prerender_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderLinkManager* prerender_link_manager =
      PrerenderLinkManagerFactory::GetForProfile(profile_);
  if (!prerender_link_manager)
    return;
  prerender_link_manager->OnAbandonPrerender(render_process_id_, prerender_id);
}

}  // namespace prerender

