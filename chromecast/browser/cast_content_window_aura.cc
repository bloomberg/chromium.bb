// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_aura.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate,
    bool is_headless) {
  DCHECK(delegate);
  return base::WrapUnique(new CastContentWindowAura());
}

CastContentWindowAura::CastContentWindowAura() {}
CastContentWindowAura::~CastContentWindowAura() {}

void CastContentWindowAura::ShowWebContents(content::WebContents* web_contents,
                                            CastWindowManager* window_manager) {
  DCHECK(window_manager);
  gfx::NativeView window = web_contents->GetNativeView();
  window_manager->SetWindowId(window, CastWindowManager::APP);
  window_manager->AddWindow(window);
  window->Show();
}

}  // namespace shell
}  // namespace chromecast
