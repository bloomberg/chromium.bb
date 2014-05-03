// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/web_contents_sizer.h"

#include "content/public/browser/web_contents.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#elif defined(OS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#endif

namespace apps {

void ResizeWebContents(content::WebContents* web_contents,
                       const gfx::Size& new_size) {
#if defined(USE_AURA)
  aura::Window* window = web_contents->GetNativeView();
  window->SetBounds(gfx::Rect(window->bounds().origin(), new_size));
#elif defined(OS_ANDROID)
  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (view)
    view->SetSize(new_size);
#endif
}

}  // namespace apps
