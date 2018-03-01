// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_aura.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* /* delegate */,
    bool /* is_headless */,
    bool /* enable_touch_input */) {
  return base::WrapUnique(new CastContentWindowAura());
}

CastContentWindowAura::CastContentWindowAura() = default;
CastContentWindowAura::~CastContentWindowAura() = default;

void CastContentWindowAura::CreateWindowForWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager,
    bool is_visible) {
  DCHECK(web_contents);
  DCHECK(window_manager);
  gfx::NativeView window = web_contents->GetNativeView();
  window_manager->SetWindowId(window, CastWindowManager::APP);
  window_manager->AddWindow(window);
  if (is_visible) {
    window->Show();
  } else {
    window->Hide();
  }
}

}  // namespace shell
}  // namespace chromecast
