// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

#include "base/memory/scoped_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Animation;
class Image;
}  // namespace gfx

// Media state for a tab.  In reality, more than one of these may apply.  See
// comments for GetTabMediaStateForContents() below.
enum TabMediaState {
  TAB_MEDIA_STATE_NONE,
  TAB_MEDIA_STATE_RECORDING,  // Audio/Video being recorded, consumed by tab.
  TAB_MEDIA_STATE_CAPTURING,  // Tab contents being captured.
  TAB_MEDIA_STATE_AUDIO_PLAYING  // Audible audio is playing from the tab.
};

namespace chrome {

// Logic to determine which components (i.e., close button, favicon, and media
// indicator) of a tab should be shown, given current state.  |capacity|
// specifies how many components can be shown, given available tab width.
//
// Precedence rules for deciding what to show when capacity is insufficient to
// show everything:
//
//   Active tab: Always show the close button, then the capture/recording
//               indicator, then favicon, then audio playback indicator.
//   Inactive tab: Capture/Recording indicator, then favicon, then audio
//                 playback indicator, then close button.
//   Pinned tab: Capture/recording indicator, then favicon, then audio
//               playback indicator.  Never show the close button.
bool ShouldTabShowFavicon(int capacity,
                          bool is_pinned_tab,
                          bool is_active_tab,
                          bool has_favicon,
                          TabMediaState media_state);
bool ShouldTabShowMediaIndicator(int capacity,
                                 bool is_pinned_tab,
                                 bool is_active_tab,
                                 bool has_favicon,
                                 TabMediaState media_state);
bool ShouldTabShowCloseButton(int capacity,
                              bool is_pinned_tab,
                              bool is_active_tab);

// Returns whether the given |contents| is playing audio. We might choose to
// show an audio favicon indicator for this tab.
bool IsPlayingAudio(content::WebContents* contents);

// Returns the media state to be shown by the tab's media indicator.  When
// multiple states apply (e.g., tab capture with audio playback), the one most
// relevant to user privacy concerns is selected.
TabMediaState GetTabMediaStateForContents(content::WebContents* contents);

// Returns a cached image, to be shown by the media indicator for the given
// |media_state|.  Uses the global ui::ResourceBundle shared instance.
const gfx::Image& GetTabMediaIndicatorImage(TabMediaState media_state);

// Returns a non-continuous Animation that performs a fade-in or fade-out
// appropriate for the given |next_media_state|.  This is used by the tab media
// indicator to alert the user that recording, tab capture, or audio playback
// has started/stopped.
scoped_ptr<gfx::Animation> CreateTabMediaIndicatorFadeAnimation(
    TabMediaState next_media_state);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
