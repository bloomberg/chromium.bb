// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

#if !defined(OS_MACOSX)
#include "ui/gfx/paint_vector_icon.h"
#endif

struct LastMuteMetadata
    : public content::WebContentsUserData<LastMuteMetadata> {
  TabMutedReason reason = TAB_MUTED_REASON_NONE;
  std::string extension_id;

 private:
  explicit LastMuteMetadata(content::WebContents* contents) {}
  friend class content::WebContentsUserData<LastMuteMetadata>;
};

DEFINE_WEB_CONTENTS_USER_DATA_KEY(LastMuteMetadata);

namespace chrome {

namespace {

// Interval between frame updates of the tab indicator animations.  This is not
// the usual 60 FPS because a trade-off must be made between tab UI animation
// smoothness and media recording/playback performance on low-end hardware.
const int kIndicatorFrameIntervalMs = 50;  // 20 FPS

// Fade-in/out duration for the tab indicator animations.  Fade-in is quick to
// immediately notify the user.  Fade-out is more gradual, so that the user has
// a chance of finding a tab that has quickly "blipped" on and off.
const int kIndicatorFadeInDurationMs = 200;
const int kIndicatorFadeOutDurationMs = 1000;

// Animation that throbs in (towards 1.0) and out (towards 0.0), and ends in the
// "in" state.
class TabRecordingIndicatorAnimation : public gfx::MultiAnimation {
 public:
  ~TabRecordingIndicatorAnimation() override {}

  // Overridden to provide alternating "towards in" and "towards out" behavior.
  double GetCurrentValue() const override;

  static scoped_ptr<TabRecordingIndicatorAnimation> Create();

 private:
  TabRecordingIndicatorAnimation(const gfx::MultiAnimation::Parts& parts,
                                 const base::TimeDelta interval)
      : MultiAnimation(parts, interval) {}

  // Number of times to "toggle throb" the recording and tab capture indicators
  // when they first appear.
  static const int kCaptureIndicatorThrobCycles = 5;
};

double TabRecordingIndicatorAnimation::GetCurrentValue() const {
  return current_part_index() % 2 ?
      1.0 - MultiAnimation::GetCurrentValue() :
      MultiAnimation::GetCurrentValue();
}

scoped_ptr<TabRecordingIndicatorAnimation>
TabRecordingIndicatorAnimation::Create() {
  MultiAnimation::Parts parts;
  static_assert(kCaptureIndicatorThrobCycles % 2 != 0,
        "odd number of cycles required so animation finishes in showing state");
  for (int i = 0; i < kCaptureIndicatorThrobCycles; ++i) {
    parts.push_back(MultiAnimation::Part(
        i % 2 ? kIndicatorFadeOutDurationMs : kIndicatorFadeInDurationMs,
        gfx::Tween::EASE_IN));
  }
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kIndicatorFrameIntervalMs);
  scoped_ptr<TabRecordingIndicatorAnimation> animation(
      new TabRecordingIndicatorAnimation(parts, interval));
  animation->set_continuous(false);
  return animation.Pass();
}

}  // namespace

bool ShouldTabShowFavicon(int capacity,
                          bool is_pinned_tab,
                          bool is_active_tab,
                          bool has_favicon,
                          TabMediaState media_state) {
  if (!has_favicon)
    return false;
  int required_capacity = 1;
  if (ShouldTabShowCloseButton(capacity, is_pinned_tab, is_active_tab))
    ++required_capacity;
  if (ShouldTabShowMediaIndicator(
          capacity, is_pinned_tab, is_active_tab, has_favicon, media_state)) {
    ++required_capacity;
  }
  return capacity >= required_capacity;
}

bool ShouldTabShowMediaIndicator(int capacity,
                                 bool is_pinned_tab,
                                 bool is_active_tab,
                                 bool has_favicon,
                                 TabMediaState media_state) {
  if (media_state == TAB_MEDIA_STATE_NONE)
    return false;
  if (ShouldTabShowCloseButton(capacity, is_pinned_tab, is_active_tab))
    return capacity >= 2;
  return capacity >= 1;
}

bool ShouldTabShowCloseButton(int capacity,
                              bool is_pinned_tab,
                              bool is_active_tab) {
  if (is_pinned_tab)
    return false;
  else if (is_active_tab)
    return true;
  else
    return capacity >= 3;
}

TabMediaState GetTabMediaStateForContents(content::WebContents* contents) {
  if (!contents)
    return TAB_MEDIA_STATE_NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  if (indicator.get()) {
    if (indicator->IsBeingMirrored(contents))
      return TAB_MEDIA_STATE_CAPTURING;
    if (indicator->IsCapturingUserMedia(contents))
      return TAB_MEDIA_STATE_RECORDING;
  }

  if (contents->IsAudioMuted())
    return TAB_MEDIA_STATE_AUDIO_MUTING;
  if (contents->WasRecentlyAudible())
    return TAB_MEDIA_STATE_AUDIO_PLAYING;

  return TAB_MEDIA_STATE_NONE;
}

gfx::Image GetTabMediaIndicatorImage(TabMediaState media_state,
                                     SkColor button_color) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  switch (media_state) {
#if !defined(OS_MACOSX)
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
    case TAB_MEDIA_STATE_AUDIO_MUTING: {
      return gfx::Image(
          gfx::CreateVectorIcon(media_state == TAB_MEDIA_STATE_AUDIO_PLAYING
                                    ? gfx::VectorIconId::TAB_AUDIO
                                    : gfx::VectorIconId::TAB_AUDIO_MUTING,
                                16, button_color));
    }
#else
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
      return rb->GetNativeImageNamed(IDR_TAB_AUDIO_INDICATOR);
    case TAB_MEDIA_STATE_AUDIO_MUTING:
      return rb->GetNativeImageNamed(IDR_TAB_AUDIO_MUTING_INDICATOR);
#endif
    case TAB_MEDIA_STATE_RECORDING:
      return rb->GetNativeImageNamed(IDR_TAB_RECORDING_INDICATOR);
    case TAB_MEDIA_STATE_CAPTURING:
      return rb->GetNativeImageNamed(IDR_TAB_CAPTURE_INDICATOR);
    case TAB_MEDIA_STATE_NONE:
      break;
  }
  NOTREACHED();
  return gfx::Image();
}

gfx::Image GetTabMediaIndicatorAffordanceImage(TabMediaState media_state,
                                               SkColor button_color) {
  switch (media_state) {
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
      return GetTabMediaIndicatorImage(TAB_MEDIA_STATE_AUDIO_MUTING,
                                       button_color);
    case TAB_MEDIA_STATE_AUDIO_MUTING:
    case TAB_MEDIA_STATE_NONE:
    case TAB_MEDIA_STATE_RECORDING:
    case TAB_MEDIA_STATE_CAPTURING:
      return GetTabMediaIndicatorImage(media_state, button_color);
  }
  NOTREACHED();
  return GetTabMediaIndicatorImage(media_state, button_color);
}

scoped_ptr<gfx::Animation> CreateTabMediaIndicatorFadeAnimation(
    TabMediaState media_state) {
  if (media_state == TAB_MEDIA_STATE_RECORDING ||
      media_state == TAB_MEDIA_STATE_CAPTURING) {
    return TabRecordingIndicatorAnimation::Create();
  }

  // Note: While it seems silly to use a one-part MultiAnimation, it's the only
  // gfx::Animation implementation that lets us control the frame interval.
  gfx::MultiAnimation::Parts parts;
  const bool is_for_fade_in = (media_state != TAB_MEDIA_STATE_NONE);
  parts.push_back(gfx::MultiAnimation::Part(
      is_for_fade_in ? kIndicatorFadeInDurationMs : kIndicatorFadeOutDurationMs,
      gfx::Tween::EASE_IN));
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kIndicatorFrameIntervalMs);
  scoped_ptr<gfx::MultiAnimation> animation(
      new gfx::MultiAnimation(parts, interval));
  animation->set_continuous(false);
  return animation.Pass();
}

base::string16 AssembleTabTooltipText(const base::string16& title,
                                      TabMediaState media_state) {
  if (media_state == TAB_MEDIA_STATE_NONE)
    return title;

  base::string16 result = title;
  if (!result.empty())
    result.append(1, '\n');
  switch (media_state) {
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_AUDIO_PLAYING));
      break;
    case TAB_MEDIA_STATE_AUDIO_MUTING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_AUDIO_MUTING));
      break;
    case TAB_MEDIA_STATE_RECORDING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_RECORDING));
      break;
    case TAB_MEDIA_STATE_CAPTURING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_CAPTURING));
      break;
    case TAB_MEDIA_STATE_NONE:
      NOTREACHED();
      break;
  }
  return result;
}

bool AreExperimentalMuteControlsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTabAudioMuting);
}

bool CanToggleAudioMute(content::WebContents* contents) {
  switch (GetTabMediaStateForContents(contents)) {
    case TAB_MEDIA_STATE_NONE:
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
    case TAB_MEDIA_STATE_AUDIO_MUTING:
      return true;
    case TAB_MEDIA_STATE_RECORDING:
    case TAB_MEDIA_STATE_CAPTURING:
      return false;
  }
  NOTREACHED();
  return false;
}

TabMutedReason GetTabAudioMutedReason(content::WebContents* contents) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  if (GetTabMediaStateForContents(contents) == TAB_MEDIA_STATE_CAPTURING) {
    // For tab capture, libcontent forces muting off.
    metadata->reason = TAB_MUTED_REASON_MEDIA_CAPTURE;
    metadata->extension_id.clear();
  }
  return metadata->reason;
}

const std::string& GetExtensionIdForMutedTab(content::WebContents* contents) {
  DCHECK_EQ(GetTabAudioMutedReason(contents) != TAB_MUTED_REASON_EXTENSION,
            LastMuteMetadata::FromWebContents(contents)->extension_id.empty());
  return LastMuteMetadata::FromWebContents(contents)->extension_id;
}

TabMutedResult SetTabAudioMuted(content::WebContents* contents,
                                bool mute,
                                TabMutedReason reason,
                                const std::string& extension_id) {
  DCHECK(contents);
  DCHECK_NE(TAB_MUTED_REASON_NONE, reason);

  if (reason == TAB_MUTED_REASON_AUDIO_INDICATOR &&
      !AreExperimentalMuteControlsEnabled()) {
    return TAB_MUTED_RESULT_FAIL_NOT_ENABLED;
  }

  if (!chrome::CanToggleAudioMute(contents))
    return TAB_MUTED_RESULT_FAIL_TABCAPTURE;

  contents->SetAudioMuted(mute);

  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  metadata->reason = reason;
  if (reason == TAB_MUTED_REASON_EXTENSION) {
    DCHECK(!extension_id.empty());
    metadata->extension_id = extension_id;
  } else {
    metadata->extension_id.clear();
  }

  return TAB_MUTED_RESULT_SUCCESS;
}

void UnmuteIfMutedByExtension(content::WebContents* contents,
                              const std::string& extension_id) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  if (metadata->reason == TAB_MUTED_REASON_EXTENSION &&
      metadata->extension_id == extension_id) {
    SetTabAudioMuted(contents, false, TAB_MUTED_REASON_EXTENSION, extension_id);
  }
}

bool AreAllTabsMuted(const TabStripModel& tab_strip,
                     const std::vector<int>& indices) {
  for (std::vector<int>::const_iterator i = indices.begin(); i != indices.end();
       ++i) {
    if (!tab_strip.GetWebContentsAt(*i)->IsAudioMuted())
      return false;
  }
  return true;
}

}  // namespace chrome
