// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window.h"

#include "base/threading/thread_restrictions.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace chromecast {

#if defined(USE_AURA)
class CastFillLayout : public aura::LayoutManager {
 public:
  explicit CastFillLayout(aura::Window* root) : root_(root) {}
  virtual ~CastFillLayout() {}

 private:
  virtual void OnWindowResized() override {}

  virtual void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(root_->bounds());
  }

  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) override {}

  virtual void OnWindowRemovedFromLayout(aura::Window* child) override {}

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) override {}

  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(CastFillLayout);
};
#endif

CastContentWindow::CastContentWindow() {}

CastContentWindow::~CastContentWindow() {
#if defined(USE_AURA)
  window_tree_host_.reset();
  // We don't delete the screen here to avoid a CHECK failure when
  // the screen size is queried periodically for metric gathering. b/18101124
#endif
}

scoped_ptr<content::WebContents> CastContentWindow::Create(
    const gfx::Size& initial_size,
    content::BrowserContext* browser_context) {
#if defined(USE_AURA)
  // Aura initialization
  // TODO(lcwu): We only need a minimal implementation of gfx::Screen
  // and aura's TestScreen will do for us now. We should change to use
  // ozone's screen implementation when it is ready.
  gfx::Screen* old_screen =
      gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE);
  if (!old_screen || old_screen->GetPrimaryDisplay().size() != initial_size) {
    gfx::Screen* new_screen = aura::TestScreen::Create(initial_size);
    DCHECK(new_screen) << "New screen not created.";
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, new_screen);
    if (old_screen) {
      delete old_screen;
    }
  }
  CHECK(aura::Env::GetInstance());
  window_tree_host_.reset(
      aura::WindowTreeHost::Create(gfx::Rect(initial_size)));
  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(
      new CastFillLayout(window_tree_host_->window()));
  window_tree_host_->Show();
#endif

  content::WebContents::CreateParams create_params(browser_context, NULL);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = initial_size;
  content::WebContents* web_contents = content::WebContents::Create(
      create_params);

#if defined(USE_AURA)
  // Add and show content's view/window
  aura::Window* content_window = web_contents->GetNativeView();
  aura::Window* parent = window_tree_host_->window();
  if (!parent->Contains(content_window)) {
    parent->AddChild(content_window);
  }
  content_window->Show();
#endif

  return make_scoped_ptr(web_contents);
}

}  // namespace chromecast
