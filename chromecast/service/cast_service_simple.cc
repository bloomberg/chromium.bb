// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/service/cast_service_simple.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace chromecast {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = command_line->GetArgs();

  if (args.empty())
    return GURL("http://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(base::FilePath(args[0]));
}

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root) : root_(root) {}
  virtual ~FillLayout() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}

  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    child->SetBounds(root_->bounds());
  }

  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}

  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

}  // namespace

// static
CastService* CastService::Create(
    content::BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context_getter) {
  return new CastServiceSimple(browser_context);
}

CastServiceSimple::CastServiceSimple(content::BrowserContext* browser_context)
    : CastService(browser_context) {
}

CastServiceSimple::~CastServiceSimple() {
}

void CastServiceSimple::Initialize() {
}

void CastServiceSimple::StartInternal() {
  // Aura initialization
  gfx::Size initial_size = gfx::Size(1280, 720);
  // TODO(lcwu): http://crbug.com/391074. Chromecast only needs a minimal
  // implementation of gfx::screen and aura's TestScreen will do for now.
  // Change the code to use ozone's screen implementation when it is ready.
  aura::TestScreen* screen = aura::TestScreen::Create(initial_size);
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen);
  CHECK(aura::Env::GetInstance());
  window_tree_host_.reset(
      aura::WindowTreeHost::Create(gfx::Rect(initial_size)));
  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(
      new FillLayout(window_tree_host_->window()));
  window_tree_host_->Show();

  // Create a WebContents
  content::WebContents::CreateParams create_params(browser_context(), NULL);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = initial_size;
  web_contents_.reset(content::WebContents::Create(create_params));

  // Add and show content's view/window
  aura::Window* content_window = web_contents_->GetNativeView();
  aura::Window* parent = window_tree_host_->window();
  if (!parent->Contains(content_window)) {
    parent->AddChild(content_window);
  }
  content_window->Show();

  web_contents_->GetController().LoadURL(GetStartupURL(),
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED,
                                         std::string());
}

void CastServiceSimple::StopInternal() {
  web_contents_->GetRenderViewHost()->ClosePage();
  window_tree_host_.reset();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, NULL);
  aura::Env::DeleteInstance();
  web_contents_.reset();
}

}  // namespace chromecast
