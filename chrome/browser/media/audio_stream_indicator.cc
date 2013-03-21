// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_stream_indicator.h"

#include "base/bind.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::WebContents;

AudioStreamIndicator::AudioStreamIndicator() {}
AudioStreamIndicator::~AudioStreamIndicator() {}

void AudioStreamIndicator::UpdateWebContentsStatus(int render_process_id,
                                                   int render_view_id,
                                                   int stream_id,
                                                   bool playing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AudioStreamIndicator::UpdateWebContentsStatusOnUIThread,
                 this, render_process_id, render_view_id, stream_id, playing));
}

bool AudioStreamIndicator::IsPlayingAudio(WebContents* contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewId id(contents->GetRenderProcessHost()->GetID(),
                  contents->GetRenderViewHost()->GetRoutingID());
  return audio_streams_.find(id) != audio_streams_.end();
}

AudioStreamIndicator::RenderViewId::RenderViewId(int render_process_id,
                                                 int render_view_id)
    : render_process_id(render_process_id),
      render_view_id(render_view_id) {
}

bool AudioStreamIndicator::RenderViewId::operator<(
    const RenderViewId& other) const {
  if (render_process_id != other.render_process_id)
    return render_process_id < other.render_process_id;

  return render_view_id < other.render_view_id;
}

void AudioStreamIndicator::UpdateWebContentsStatusOnUIThread(
    int render_process_id,
    int render_view_id,
    int stream_id,
    bool playing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewId id(render_process_id, render_view_id);
  if (playing) {
    audio_streams_[id].insert(stream_id);
  } else {
    std::map<RenderViewId, std::set<int> >::iterator it =
        audio_streams_.find(id);
    if (it == audio_streams_.end())
      return;

    it->second.erase(stream_id);
    if (it->second.empty())
      audio_streams_.erase(it);
  }

  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);
  if (web_contents)
    web_contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}
