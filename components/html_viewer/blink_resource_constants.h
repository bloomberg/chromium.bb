// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_

#include "blink/public/resources/grit/blink_image_resources.h"
#include "blink/public/resources/grit/blink_resources.h"

namespace html_viewer {

struct DataResource {
  const char* name;
  int id;
};

const DataResource kDataResources[] = {
    {"missingImage", IDR_BROKENIMAGE},
    // Skipping missingImage@2x
    {"mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON},
    {"mediaplayerPauseHover", IDR_MEDIAPLAYER_PAUSE_BUTTON_HOVER},
    {"mediaplayerPauseDown", IDR_MEDIAPLAYER_PAUSE_BUTTON_DOWN},
    {"mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON},
    {"mediaplayerPlayHover", IDR_MEDIAPLAYER_PLAY_BUTTON_HOVER},
    {"mediaplayerPlayDown", IDR_MEDIAPLAYER_PLAY_BUTTON_DOWN},
    {"mediaplayerPlayDisabled", IDR_MEDIAPLAYER_PLAY_BUTTON_DISABLED},
    {"mediaplayerSoundLevel3", IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON},
    {"mediaplayerSoundLevel3Hover", IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_HOVER},
    {"mediaplayerSoundLevel3Down", IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_DOWN},
    {"mediaplayerSoundLevel2", IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON},
    {"mediaplayerSoundLevel2Hover", IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_HOVER},
    {"mediaplayerSoundLevel2Down", IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_DOWN},
    {"mediaplayerSoundLevel1", IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON},
    {"mediaplayerSoundLevel1Hover", IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_HOVER},
    {"mediaplayerSoundLevel1Down", IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_DOWN},
    {"mediaplayerSoundLevel0", IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON},
    {"mediaplayerSoundLevel0Hover", IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_HOVER},
    {"mediaplayerSoundLevel0Down", IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_DOWN},
    {"mediaplayerSoundDisabled", IDR_MEDIAPLAYER_SOUND_DISABLED},
    {"mediaplayerSliderThumb", IDR_MEDIAPLAYER_SLIDER_THUMB},
    {"mediaplayerSliderThumbHover", IDR_MEDIAPLAYER_SLIDER_THUMB_HOVER},
    {"mediaplayerSliderThumbDown", IDR_MEDIAPLAYER_SLIDER_THUMB_DOWN},
    {"mediaplayerVolumeSliderThumb", IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB},
    {"mediaplayerVolumeSliderThumbHover",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_HOVER},
    {"mediaplayerVolumeSliderThumbDown",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DOWN},
    {"mediaplayerVolumeSliderThumbDisabled",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DISABLED},
    {"mediaplayerClosedCaption", IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON},
    {"mediaplayerClosedCaptionHover",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_HOVER},
    {"mediaplayerClosedCaptionDown",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DOWN},
    {"mediaplayerClosedCaptionDisabled",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED},
    {"mediaplayerFullscreen", IDR_MEDIAPLAYER_FULLSCREEN_BUTTON},
    {"mediaplayerFullscreenHover", IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_HOVER},
    {"mediaplayerFullscreenDown", IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DOWN},
    {"mediaplayerCastOff", IDR_MEDIAPLAYER_CAST_BUTTON_OFF},
    {"mediaplayerCastOn", IDR_MEDIAPLAYER_CAST_BUTTON_ON},
    {"mediaplayerFullscreenDisabled",
     IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DISABLED},
    {"mediaplayerOverlayCastOff", IDR_MEDIAPLAYER_OVERLAY_CAST_BUTTON_OFF},
    {"mediaplayerOverlayPlay", IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON},
    {"panIcon", IDR_PAN_SCROLL_ICON},
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
    {"themeChromium.css", IDR_UASTYLE_THEME_CHROMIUM_CSS},
#if defined(OS_ANDROID)
    {"themeChromiumAndroid.css", IDR_UASTYLE_THEME_CHROMIUM_ANDROID_CSS},
    {"mediaControlsAndroid.css", IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_CSS},
#endif
#if !defined(OS_WIN)
    {"themeChromiumLinux.css", IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS},
#endif
    {"themeChromiumSkia.css", IDR_UASTYLE_THEME_CHROMIUM_SKIA_CSS},
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
    {"PluginPlaceholderElement.js",
     IDR_PRIVATE_SCRIPT_PLUGINPLACEHOLDERELEMENT_JS},
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
