// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/distiller_page_web_contents.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace dom_distiller {

scoped_ptr<DistillerPage> DistillerPageWebContentsFactory::CreateDistillerPage()
    const {
  DCHECK(browser_context_);
  return scoped_ptr<DistillerPage>(
      new DistillerPageWebContents(browser_context_));
}

DistillerPageWebContents::DistillerPageWebContents(
    content::BrowserContext* browser_context)
    : state_(IDLE), browser_context_(browser_context) {}

DistillerPageWebContents::~DistillerPageWebContents() {}

void DistillerPageWebContents::DistillPageImpl(const GURL& gurl,
                                               const std::string& script) {
  DCHECK(browser_context_);
  DCHECK(state_ == IDLE);
  state_ = LOADING_PAGE;
  script_ = script;

  // Create new WebContents to use for distilling the content.
  content::WebContents::CreateParams create_params(browser_context_);
  create_params.initially_hidden = true;
  web_contents_.reset(content::WebContents::Create(create_params));
  DCHECK(web_contents_.get());

  // Start observing WebContents and load the requested URL.
  content::WebContentsObserver::Observe(web_contents_.get());
  content::NavigationController::LoadURLParams params(gurl);
  web_contents_->GetController().LoadURLWithParams(params);
}

void DistillerPageWebContents::DocumentLoadedInFrame(
    int64 frame_id,
    RenderViewHost* render_view_host) {
  if (frame_id == web_contents_->GetMainFrame()->GetRoutingID()) {
    content::WebContentsObserver::Observe(NULL);
    web_contents_->Stop();
    ExecuteJavaScript();
  }
}

void DistillerPageWebContents::DidFailLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    int error_code,
    const base::string16& error_description,
    RenderViewHost* render_view_host) {
  if (is_main_frame) {
    content::WebContentsObserver::Observe(NULL);
    DCHECK(state_ == LOADING_PAGE || state_ == EXECUTING_JAVASCRIPT);
    state_ = PAGELOAD_FAILED;
    scoped_ptr<base::Value> empty(base::Value::CreateNullValue());
    OnWebContentsDistillationDone(GURL(), empty.get());
  }
}

void DistillerPageWebContents::ExecuteJavaScript() {
  content::RenderFrameHost* frame = web_contents_->GetMainFrame();
  DCHECK(frame);
  DCHECK_EQ(LOADING_PAGE, state_);
  state_ = EXECUTING_JAVASCRIPT;
  DVLOG(1) << "Beginning distillation";
  frame->ExecuteJavaScript(
      base::UTF8ToUTF16(script_),
      base::Bind(&DistillerPageWebContents::OnWebContentsDistillationDone,
                 base::Unretained(this),
                 web_contents_->GetLastCommittedURL()));
}

void DistillerPageWebContents::OnWebContentsDistillationDone(
    const GURL& page_url,
    const base::Value* value) {
  DCHECK(state_ == PAGELOAD_FAILED || state_ == EXECUTING_JAVASCRIPT);
  state_ = IDLE;
  DistillerPage::OnDistillationDone(page_url, value);
}

}  // namespace dom_distiller
