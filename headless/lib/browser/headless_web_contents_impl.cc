// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_web_contents_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "ui/aura/window.h"

namespace headless {

class WebContentsObserverAdapter : public content::WebContentsObserver {
 public:
  WebContentsObserverAdapter(content::WebContents* web_contents,
                             HeadlessWebContents::Observer* observer)
      : content::WebContentsObserver(web_contents), observer_(observer) {}

  ~WebContentsObserverAdapter() override {}

  void DocumentOnLoadCompletedInMainFrame() override {
    observer_->DocumentOnLoadCompletedInMainFrame();
  }

 private:
  HeadlessWebContents::Observer* observer_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverAdapter);
};

class HeadlessWebContentsImpl::Delegate : public content::WebContentsDelegate {
 public:
  Delegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

HeadlessWebContentsImpl::HeadlessWebContentsImpl(
    content::BrowserContext* browser_context,
    aura::Window* parent_window,
    const gfx::Size& initial_size)
    : web_contents_delegate_(new HeadlessWebContentsImpl::Delegate()) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  create_params.initial_size = initial_size;

  web_contents_.reset(content::WebContents::Create(create_params));
  web_contents_->SetDelegate(web_contents_delegate_.get());

  aura::Window* contents = web_contents_->GetNativeView();
  DCHECK(!parent_window->Contains(contents));
  parent_window->AddChild(contents);
  contents->Show();

  contents->SetBounds(gfx::Rect(initial_size));
  content::RenderWidgetHostView* host_view =
      web_contents_->GetRenderWidgetHostView();
  if (host_view)
    host_view->SetSize(initial_size);
}

HeadlessWebContentsImpl::~HeadlessWebContentsImpl() {
  web_contents_->Close();
}

bool HeadlessWebContentsImpl::OpenURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
  return true;
}

void HeadlessWebContentsImpl::AddObserver(Observer* observer) {
  DCHECK(observer_map_.find(observer) == observer_map_.end());
  observer_map_[observer] = make_scoped_ptr(
      new WebContentsObserverAdapter(web_contents_.get(), observer));
}

void HeadlessWebContentsImpl::RemoveObserver(Observer* observer) {
  ObserverMap::iterator it = observer_map_.find(observer);
  DCHECK(it != observer_map_.end());
  observer_map_.erase(it);
}

content::WebContents* HeadlessWebContentsImpl::web_contents() const {
  return web_contents_.get();
}

}  // namespace headless
