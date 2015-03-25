// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_
#define MOJO_SERVICES_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_

#include "blink/public/resources/grit/blink_resources.h"

namespace html_viewer {

struct DataResource {
  const char* name;
  int id;
};

const DataResource kDataResources[] = {
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
    {"navigationTransitions.css", IDR_UASTYLE_NAVIGATION_TRANSITIONS_CSS},
    {"mathml.css", IDR_UASTYLE_MATHML_CSS},
    {"mediaControls.css", IDR_UASTYLE_MEDIA_CONTROLS_CSS},
    {"fullscreen.css", IDR_UASTYLE_FULLSCREEN_CSS},
    {"xhtmlmp.css", IDR_UASTYLE_XHTMLMP_CSS},
    {"viewportAndroid.css", IDR_UASTYLE_VIEWPORT_ANDROID_CSS},
    {"InspectorOverlayPage.html", IDR_INSPECTOR_OVERLAY_PAGE_HTML},
    {"InjectedScriptCanvasModuleSource.js",
     IDR_INSPECTOR_INJECTED_SCRIPT_CANVAS_MODULE_SOURCE_JS},
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

#endif // MOJO_SERVICES_HTML_VIEWER_BLINK_RESOURCE_CONSTANTS_H_
