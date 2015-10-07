// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_window.h"

#include "base/strings/string_util.h"
#include "blimp/engine/browser/blimp_browser_main_parts.h"
#include "blimp/engine/browser/blimp_content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace blimp {
namespace engine {

namespace {
const int kDefaultWindowWidthDip = 1;
const int kDefaultWindowHeightDip = 1;

// TODO(haibinlu): cleanup BlimpWindows on shutdown. See crbug/540498.
typedef std::vector<BlimpWindow*> BlimpWindows;
base::LazyInstance<BlimpWindows>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

// Returns the default size if |size| has 0 for width and/or height;
// otherwise, returns |size|.
gfx::Size AdjustWindowSize(const gfx::Size& size) {
  return size.IsEmpty()
             ? gfx::Size(kDefaultWindowWidthDip, kDefaultWindowHeightDip)
             : size;
}
}  // namespace

BlimpWindow::~BlimpWindow() {
  BlimpWindows* instances = g_instances.Pointer();
  BlimpWindows::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);
}

// static
void BlimpWindow::Create(content::BrowserContext* browser_context,
                         const GURL& url,
                         content::SiteInstance* site_instance,
                         const gfx::Size& initial_size) {
  content::WebContents::CreateParams create_params(browser_context,
                                                   site_instance);
  scoped_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  BlimpWindow* win = DoCreate(web_contents.Pass(), initial_size);
  if (!url.is_empty())
    win->LoadURL(url);
}

void BlimpWindow::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void BlimpWindow::AddNewContents(content::WebContents* source,
                                 content::WebContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_rect,
                                 bool user_gesture,
                                 bool* was_blocked) {
  DoCreate(scoped_ptr<content::WebContents>(new_contents), initial_rect.size());
}

content::WebContents* BlimpWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // CURRENT_TAB is the only one we implement for now.
  if (params.disposition != CURRENT_TAB)
    return nullptr;
  // TOOD(haibinlu): add helper method to get LoadURLParams from OpenURLParams.
  content::NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.referrer = params.referrer;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.transition_type = params.transition;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;

  if (params.transferred_global_request_id != content::GlobalRequestID()) {
    load_url_params.is_renderer_initiated = params.is_renderer_initiated;
    load_url_params.transferred_global_request_id =
        params.transferred_global_request_id;
  } else if (params.is_renderer_initiated) {
    load_url_params.is_renderer_initiated = true;
  }

  source->GetController().LoadURLWithParams(load_url_params);
  return source;
}

void BlimpWindow::RequestToLockMouse(content::WebContents* web_contents,
                                     bool user_gesture,
                                     bool last_unlocked_by_target) {
  web_contents->GotResponseToLockMouseRequest(true);
}

void BlimpWindow::CloseContents(content::WebContents* source) {
  delete this;
}

void BlimpWindow::ActivateContents(content::WebContents* contents) {
  contents->GetRenderViewHost()->Focus();
}

void BlimpWindow::DeactivateContents(content::WebContents* contents) {
  contents->GetRenderViewHost()->Blur();
}

BlimpWindow::BlimpWindow(scoped_ptr<content::WebContents> web_contents)
    : web_contents_(web_contents.Pass()) {
  web_contents_->SetDelegate(this);
  g_instances.Get().push_back(this);
}

// static
BlimpWindow* BlimpWindow::DoCreate(
    scoped_ptr<content::WebContents> web_contents,
    const gfx::Size& initial_size) {
  BlimpWindow* win = new BlimpWindow(web_contents.Pass());
  content::RenderWidgetHostView* host_view =
      win->web_contents_->GetRenderWidgetHostView();
  if (host_view)
    host_view->SetSize(AdjustWindowSize(initial_size));
  return win;
}

}  // namespace engine
}  // namespace blimp
