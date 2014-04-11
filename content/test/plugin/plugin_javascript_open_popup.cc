// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_javascript_open_popup.h"

#include "build/build_config.h"
#include "base/logging.h"

#include "content/test/plugin/plugin_client.h"

namespace NPAPIClient {

ExecuteJavascriptOpenPopupWithPluginTest::
    ExecuteJavascriptOpenPopupWithPluginTest(NPP id,
                                             NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      popup_window_test_started_(false) {
}

int16 ExecuteJavascriptOpenPopupWithPluginTest::SetWindow(
    NPWindow* window) {
#if !defined(OS_MACOSX)
  if (window->window == NULL)
    return NPERR_NO_ERROR;
#endif

  if (!popup_window_test_started_) {
    popup_window_test_started_ = true;
    HostFunctions()->geturl(
        id(), "popup_window_with_target_plugin.html", "_blank");
  }
  return NPERR_NO_ERROR;
}

// ExecuteJavascriptPopupWindowTargetPluginTest member defines.
ExecuteJavascriptPopupWindowTargetPluginTest::
    ExecuteJavascriptPopupWindowTargetPluginTest(
        NPP id, NPNetscapeFuncs* host_functions)
    : PluginTest(id, host_functions),
      test_completed_(false) {
}

int16 ExecuteJavascriptPopupWindowTargetPluginTest::SetWindow(
    NPWindow* window) {
#if !defined(OS_MACOSX)
  if (window->window == NULL)
    return NPERR_NO_ERROR;
#endif

  if (!test_completed_) {
    if (CheckWindow(window)) {
      SignalTestCompleted();
      test_completed_ = true;
    }
  }
  return PluginTest::SetWindow(window);
}

#if defined(OS_WIN)
bool ExecuteJavascriptPopupWindowTargetPluginTest::CheckWindow(
    NPWindow* window) {
  HWND window_handle = reinterpret_cast<HWND>(window->window);

  if (IsWindow(window_handle)) {
    HWND parent_window = GetParent(window_handle);
    if (!IsWindow(parent_window))
      SetError("Windowed plugin instantiated with NULL parent");
    return true;
  }

  return false;
}

#elif defined(OS_MACOSX)
bool ExecuteJavascriptPopupWindowTargetPluginTest::CheckWindow(
    NPWindow* window) {
  // TODO(port) scaffolding--replace with a real test once NPWindow is done.
  return false;
}
#endif

} // namespace NPAPIClient
