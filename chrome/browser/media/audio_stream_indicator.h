// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_STREAM_INDICATOR_H_
#define CHROME_BROWSER_MEDIA_AUDIO_STREAM_INDICATOR_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"

namespace content {
class WebContents;
}

class AudioStreamIndicator
    : public base::RefCountedThreadSafe<AudioStreamIndicator> {
 public:
  AudioStreamIndicator();

  // This method should be called on the IO thread.
  void UpdateWebContentsStatus(int render_process_id,
                               int render_view_id,
                               int stream_id,
                               bool is_playing_and_audible);

  // This method should be called on the UI thread.
  bool IsPlayingAudio(content::WebContents* contents);

 private:
  struct RenderViewId {
    RenderViewId(int render_process_id,
                 int render_view_id);

    // Required to use this struct in the std::multiset below.
    bool operator<(const RenderViewId& other) const;

    int render_process_id;
    int render_view_id;
  };

  friend class base::RefCountedThreadSafe<AudioStreamIndicator>;
  virtual ~AudioStreamIndicator();

  void UpdateWebContentsStatusOnUIThread(int render_process_id,
                                         int render_view_id,
                                         int stream_id,
                                         bool playing);

  // A map from RenderViews to sets of streams playing in them (each RenderView
  // might have more than one stream).
  std::map<RenderViewId, std::set<int> > audio_streams_;
};

#endif  // CHROME_BROWSER_MEDIA_AUDIO_STREAM_INDICATOR_H_
