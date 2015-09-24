// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkColor.h"

class TabStripModel;

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
  TAB_MEDIA_STATE_AUDIO_PLAYING,  // Audible audio is playing from the tab.
  TAB_MEDIA_STATE_AUDIO_MUTING,  // Tab audio is being muted.
};

enum TabMutedReason {
  TAB_MUTED_REASON_NONE,  // The tab has never been muted or unmuted.
  TAB_MUTED_REASON_CONTEXT_MENU,  // Mute/Unmute chosen from tab context menu.
  TAB_MUTED_REASON_AUDIO_INDICATOR,  // Mute toggled via tab-strip audio icon.
  TAB_MUTED_REASON_MEDIA_CAPTURE,  // Media recording/capture was started.
  TAB_MUTED_REASON_EXTENSION,  // Mute state changed via extension API.
};

enum TabMutedResult {
  TAB_MUTED_RESULT_SUCCESS,
  TAB_MUTED_RESULT_FAIL_NOT_ENABLED,
  TAB_MUTED_RESULT_FAIL_TABCAPTURE,
};

namespace chrome {

// Logic to determine which components (i.e., close button, favicon, and media
// indicator) of a tab should be shown, given current state.  |capacity|
// specifies how many components can be shown, given available tab width.
//
// Precedence rules for deciding what to show when capacity is insufficient to
// show everything:
//
//   Active tab: Always show the close button, then the media indicator, then
//               the favicon.
//   Inactive tab: Media indicator, then the favicon, then the close button.
//   Pinned tab: Show only the media indicator, or only the favicon
//               (TAB_MEDIA_STATE_NONE).  Never show the close button.
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

// Returns the media state to be shown by the tab's media indicator.  When
// multiple states apply (e.g., tab capture with audio playback), the one most
// relevant to user privacy concerns is selected.
TabMediaState GetTabMediaStateForContents(content::WebContents* contents);

// Returns a cached image, to be shown by the media indicator for the given
// |media_state|.  Uses the global ui::ResourceBundle shared instance.
gfx::Image GetTabMediaIndicatorImage(TabMediaState media_state,
                                     SkColor button_color);

// Returns the cached image, to be shown by the media indicator button for mouse
// hover/pressed, when the indicator is in the given |media_state|.  Uses the
// global ui::ResourceBundle shared instance.
gfx::Image GetTabMediaIndicatorAffordanceImage(TabMediaState media_state,
                                               SkColor button_color);

// Returns a non-continuous Animation that performs a fade-in or fade-out
// appropriate for the given |next_media_state|.  This is used by the tab media
// indicator to alert the user that recording, tab capture, or audio playback
// has started/stopped.
scoped_ptr<gfx::Animation> CreateTabMediaIndicatorFadeAnimation(
    TabMediaState next_media_state);

// Returns the text to show in a tab's tooltip: The contents |title|, followed
// by a break, followed by a localized string describing the |media_state|.
base::string16 AssembleTabTooltipText(const base::string16& title,
                                      TabMediaState media_state);

// Returns true if experimental audio mute controls (UI or extension API) are
// enabled.  Currently, toggling mute from a tab's context menu is the only
// non-experimental control method.
bool AreExperimentalMuteControlsEnabled();

// Returns true if audio mute can be activated/deactivated for the given
// |contents|.
bool CanToggleAudioMute(content::WebContents* contents);

// Unmute a tab if it is currently muted at the request of the extension having
// the given |extension_id|.
void UnmuteIfMutedByExtension(content::WebContents* contents,
                              const std::string& extension_id);

// Sets whether all audio output from |contents| is muted, along with the
// |reason| it is to be muted/unmuted (via UI or extension API).  When |reason|
// is TAB_MUTED_REASON_EXTENSION, |extension_id| must be provided; otherwise, it
// is ignored.
//
// If the |reason| is an experimental feature and the experiment is not enabled,
// this will have no effect and TAB_MUTED_RESULT_FAIL_NOT_ENABLED will be
// returned.
TabMutedResult SetTabAudioMuted(content::WebContents* contents,
                                bool mute,
                                TabMutedReason reason,
                                const std::string& extension_id);

// Returns the last reason a tab's mute state was changed.
TabMutedReason GetTabAudioMutedReason(content::WebContents* contents);

// If the last reason a tab's mute state was changed was due to use of the
// extension API, this returns the extension's ID string.  Otherwise, the empty
// string is returned.
const std::string& GetExtensionIdForMutedTab(content::WebContents* contents);

// Returns true if the tabs at the |indices| in |tab_strip| are all muted.
bool AreAllTabsMuted(const TabStripModel& tab_strip,
                     const std::vector<int>& indices);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
