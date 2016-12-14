// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate_view.h"

namespace content {

#if defined(OS_ANDROID)
ui::OverscrollRefreshHandler*
content::RenderViewHostDelegateView::GetOverscrollRefreshHandler() const {
  return nullptr;
}
#endif

}  //  namespace content
