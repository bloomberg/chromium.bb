// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common functions for various API implementation modules.

#ifndef CEEE_IE_BROKER_COMMON_API_MODULE_H_
#define CEEE_IE_BROKER_COMMON_API_MODULE_H_

#include <string>

#include "ceee/ie/broker/api_dispatcher.h"

#include "toolband.h"  // NOLINT

namespace common_api {

class CommonApiResult : public ApiDispatcher::InvocationResult {
 public:
  explicit CommonApiResult(int request_id)
      : ApiDispatcher::InvocationResult(request_id) {
  }

  // Returns true if the given window is an IE "server" window, i.e. a tab.
  static bool IsTabWindowClass(HWND window);

  // Returns true if the given window is a top-level IE window.
  static bool IsIeFrameClass(HWND window);

  // Returns the IE frame window at the top of the Z-order. This will generally
  // be the last window used or the new window just created.
  // @return The HWND of the top IE window.
  static HWND TopIeWindow();

  // Build the result_ value from the provided window info. It will set the
  // value if it is currently NULL, otherwise it assumes it is a ListValue and
  // adds a new Value to it.
  // @param window The window handle
  // @param window_info The info about the window to create a new value for.
  // @param populate_tabs To specify if we want to populate the tabs info.
  virtual void SetResultFromWindowInfo(HWND window,
                                       const CeeeWindowInfo& window_info,
                                       bool populate_tabs);

  // Creates a list value of all tabs in the given list.
  // @param tab_list A list of HWND and index of the tabs for which we want to
  //        create a value JSON encoded as a list of (id, index) pairs.
  // @Return A ListValue containing the individual Values for each tab info.
  virtual Value* CreateTabList(BSTR tab_list);

  // Creates a value for the given window, populating tabs if requested.
  // Sets value_ with the appropriate value content, or resets it in case of
  // errors. Also calls PostError() if there is an error and returns false.
  // The @p window parameter contrasts with CreateTabValue because we most
  // often use this function with HWND gotten without Ids (ie. from
  // TopIeWindow()). This was not the case with CreateTabValue.
  // @param window The identifier of the window for which to create the value.
  // @param populate_tabs To specify if we want to populate the tabs info.
  virtual bool CreateWindowValue(HWND window, bool populate_tabs);
};

// Helper class to handle the data cleanup.
class WindowInfo : public CeeeWindowInfo {
 public:
  WindowInfo();
  ~WindowInfo();
};

}  // namespace common_api

#endif  // CEEE_IE_BROKER_COMMON_API_MODULE_H_
