// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_linux.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // defined(USE_AURA)

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate) {
  DCHECK(delegate);
  return base::WrapUnique(new CastContentWindowLinux());
}

CastContentWindowLinux::CastContentWindowLinux() : transparent_(false) {}

CastContentWindowLinux::~CastContentWindowLinux() {}

void CastContentWindowLinux::SetTransparent() {
  transparent_ = true;
}

void CastContentWindowLinux::ShowWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager) {
  DCHECK(window_manager);
  gfx::NativeView window = web_contents->GetNativeView();
  window_manager->SetWindowId(window, CastWindowManager::APP);
  window_manager->AddWindow(window);
  window->Show();
}

}  // namespace shell
}  // namespace chromecast
