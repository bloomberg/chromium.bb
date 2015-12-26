// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_

#include "blink/public/resources/grit/blink_image_resources.h"
#include "blink/public/resources/grit/blink_resources.h"
#include "build/build_config.h"

namespace html_viewer {

struct DataResource {
  const char* name;
  int id;
};

const DataResource kDataResources[] = {
    {"missingImage", IDR_BROKENIMAGE},
    // Skipping missingImage@2x
    {"mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON},
    {"mediaplayerPauseNew", IDR_MEDIAPLAYER_PAUSE_BUTTON_NEW},
    {"mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON},
    {"mediaplayerPlayNew", IDR_MEDIAPLAYER_PLAY_BUTTON_NEW},
    {"mediaplayerPlayDisabled", IDR_MEDIAPLAYER_PLAY_BUTTON_DISABLED},
    {"mediaplayerSoundLevel3", IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON},
    {"mediaplayerSoundLevel3New", IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_NEW},
    {"mediaplayerSoundLevel2", IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON},
    {"mediaplayerSoundLevel1", IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON},
    {"mediaplayerSoundLevel0", IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON},
    {"mediaplayerSoundLevel0New", IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_NEW},
    {"mediaplayerSoundDisabled", IDR_MEDIAPLAYER_SOUND_DISABLED},
    {"mediaplayerSliderThumb", IDR_MEDIAPLAYER_SLIDER_THUMB},
    {"mediaplayerSliderThumbNew", IDR_MEDIAPLAYER_SLIDER_THUMB_NEW},
    {"mediaplayerVolumeSliderThumb", IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB},
    {"mediaplayerVolumeSliderThumbNew",
        IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_NEW},
    {"mediaplayerVolumeSliderThumbDisabled",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DISABLED},
    {"mediaplayerClosedCaption", IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON},
    {"mediaplayerClosedCaptionNew", IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_NEW},
    {"mediaplayerClosedCaptionDisabled",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED},
    {"mediaplayerClosedCaptionDisabledNew",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED_NEW},
    {"mediaplayerEnterFullscreen", IDR_MEDIAPLAYER_ENTER_FULLSCREEN_BUTTON},
    {"mediaplayerFullscreen", IDR_MEDIAPLAYER_FULLSCREEN_BUTTON},
    {"mediaplayerCastOff", IDR_MEDIAPLAYER_CAST_BUTTON_OFF},
    {"mediaplayerCastOffNew", IDR_MEDIAPLAYER_CAST_BUTTON_OFF_NEW},
    {"mediaplayerCastOn", IDR_MEDIAPLAYER_CAST_BUTTON_ON},
    {"mediaplayerCastOnNew", IDR_MEDIAPLAYER_CAST_BUTTON_ON_NEW},
    {"mediaplayerFullscreenDisabled",
     IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DISABLED},
    {"mediaplayerOverlayCastOff", IDR_MEDIAPLAYER_OVERLAY_CAST_BUTTON_OFF},
    {"mediaplayerOverlayCastOffNew",
     IDR_MEDIAPLAYER_OVERLAY_CAST_BUTTON_OFF_NEW},
    {"mediaplayerOverlayPlay", IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON},
    {"mediaplayerOverlayPlayNew", IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON_NEW},
    {"searchCancel", IDR_SEARCH_CANCEL},
    {"searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED},
    {"searchMagnifier", IDR_SEARCH_MAGNIFIER},
    {"searchMagnifierResults", IDR_SEARCH_MAGNIFIER_RESULTS},
    {"textAreaResizeCorner", IDR_TEXTAREA_RESIZER},
    // Skipping "textAreaResizeCorner@2x"
    {"generatePassword", IDR_PASSWORD_GENERATION_ICON},
    {"generatePasswordHover", IDR_PASSWORD_GENERATION_ICON_HOVER},
    {"html.css", IDR_UASTYLE_HTML_CSS},
    {"quirks.css", IDR_UASTYLE_QUIRKS_CSS},
    {"view-source.css", IDR_UASTYLE_VIEW_SOURCE_CSS},
#if defined(OS_ANDROID)
    {"themeChromiumAndroid.css", IDR_UASTYLE_THEME_CHROMIUM_ANDROID_CSS},
    {"mediaControlsAndroid.css", IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_CSS},
#endif
#if !defined(OS_WIN)
    {"themeChromiumLinux.css", IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS},
#endif
    {"themeInputMultipleFields.css",
     IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS},
#if defined(OS_MACOSX)
    {"themeMac.css", IDR_UASTYLE_THEME_MAC_CSS},
#endif
    {"themeWin.css", IDR_UASTYLE_THEME_WIN_CSS},
    {"themeWinQuirks.css", IDR_UASTYLE_THEME_WIN_QUIRKS_CSS},
    {"svg.css", IDR_UASTYLE_SVG_CSS},
    {"mathml.css", IDR_UASTYLE_MATHML_CSS},
    {"mediaControls.css", IDR_UASTYLE_MEDIA_CONTROLS_CSS},
    {"fullscreen.css", IDR_UASTYLE_FULLSCREEN_CSS},
    {"xhtmlmp.css", IDR_UASTYLE_XHTMLMP_CSS},
    {"viewportAndroid.css", IDR_UASTYLE_VIEWPORT_ANDROID_CSS},
    {"InspectorOverlayPage.html", IDR_INSPECTOR_OVERLAY_PAGE_HTML},
    {"InjectedScriptSource.js", IDR_INSPECTOR_INJECTED_SCRIPT_SOURCE_JS},
    {"DebuggerScriptSource.js", IDR_INSPECTOR_DEBUGGER_SCRIPT_SOURCE_JS},
    {"DocumentExecCommand.js", IDR_PRIVATE_SCRIPT_DOCUMENTEXECCOMMAND_JS},
    {"DocumentXMLTreeViewer.css", IDR_PRIVATE_SCRIPT_DOCUMENTXMLTREEVIEWER_CSS},
    {"DocumentXMLTreeViewer.js", IDR_PRIVATE_SCRIPT_DOCUMENTXMLTREEVIEWER_JS},
    {"HTMLMarqueeElement.js", IDR_PRIVATE_SCRIPT_HTMLMARQUEEELEMENT_JS},
    {"PrivateScriptRunner.js", IDR_PRIVATE_SCRIPT_PRIVATESCRIPTRUNNER_JS},
#ifdef IDR_PICKER_COMMON_JS
    {"pickerCommon.js", IDR_PICKER_COMMON_JS},
    {"pickerCommon.css", IDR_PICKER_COMMON_CSS},
    {"calendarPicker.js", IDR_CALENDAR_PICKER_JS},
    {"calendarPicker.css", IDR_CALENDAR_PICKER_CSS},
    {"pickerButton.css", IDR_PICKER_BUTTON_CSS},
    {"suggestionPicker.js", IDR_SUGGESTION_PICKER_JS},
    {"suggestionPicker.css", IDR_SUGGESTION_PICKER_CSS},
    {"colorSuggestionPicker.js", IDR_COLOR_SUGGESTION_PICKER_JS},
    {"colorSuggestionPicker.css", IDR_COLOR_SUGGESTION_PICKER_CSS}
#endif
};

} // namespace html_viewer

#endif // COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_
