// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ITabWindow and ITabWindowManager interfaces and approaches
// related to them.

#include "ceee/ie/common/ie_tab_interfaces.h"

#include <shlguid.h>  // SID_SWebBrowserApp

#include "base/logging.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/common/window_utils.h"

namespace ie_tab_interfaces {

HRESULT TabWindowManagerFromFrame(HWND ie_frame, REFIID riid, void** manager) {
  DCHECK(ie_frame);
  DCHECK(manager && !*manager);

  if (!window_utils::IsWindowThread(ie_frame)) {
    LOG(ERROR) << "Can't get tab window manager from frame in other process "
                  "or thread that created the frame window.";
    return E_INVALIDARG;
  }

  IDropTarget* drop_target = reinterpret_cast<IDropTarget*>(
      ::GetPropW(ie_frame, L"OleDropTargetInterface"));
  if (!drop_target) {
    NOTREACHED() << "No drop target";
    return E_UNEXPECTED;
  }

  CComQIPtr<IServiceProvider> frame_service_provider(drop_target);
  if (!frame_service_provider) {
    NOTREACHED();
    return E_NOINTERFACE;
  }

  return frame_service_provider->QueryService(SID_STabWindowManager, riid,
                                              manager);
}

}  // namespace ie_tab_interfaces
