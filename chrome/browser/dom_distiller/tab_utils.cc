// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils.h"

#include "base/message_loop/message_loop.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

using dom_distiller::ViewRequestDelegate;
using dom_distiller::DistilledArticleProto;
using dom_distiller::ArticleDistillationUpdate;
using dom_distiller::ViewerHandle;
using dom_distiller::SourcePageHandleWebContents;
using dom_distiller::DomDistillerService;
using dom_distiller::DomDistillerServiceFactory;
using dom_distiller::DistillerPage;
using dom_distiller::SourcePageHandle;

// An no-op ViewRequestDelegate which holds a ViewerHandle and deletes itself
// after the WebContents navigates or goes away. This class is a band-aid to
// keep a TaskTracker around until the distillation starts from the viewer.
class SelfDeletingRequestDelegate : public ViewRequestDelegate,
                                    public content::WebContentsObserver {
 public:
  explicit SelfDeletingRequestDelegate(content::WebContents* web_contents);
  virtual ~SelfDeletingRequestDelegate();

  // ViewRequestDelegate implementation.
  virtual void OnArticleReady(
      const DistilledArticleProto* article_proto) OVERRIDE;
  virtual void OnArticleUpdated(
      ArticleDistillationUpdate article_update) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // Takes ownership of the ViewerHandle to keep distillation alive until |this|
  // is deleted.
  void TakeViewerHandle(scoped_ptr<ViewerHandle> viewer_handle);

 private:
  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  scoped_ptr<ViewerHandle> viewer_handle_;
};

void SelfDeletingRequestDelegate::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SelfDeletingRequestDelegate::RenderProcessGone(
    base::TerminationStatus status) {
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SelfDeletingRequestDelegate::WebContentsDestroyed() {
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

SelfDeletingRequestDelegate::SelfDeletingRequestDelegate(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

SelfDeletingRequestDelegate::~SelfDeletingRequestDelegate() {
}

void SelfDeletingRequestDelegate::OnArticleReady(
    const DistilledArticleProto* article_proto) {
}

void SelfDeletingRequestDelegate::OnArticleUpdated(
    ArticleDistillationUpdate article_update) {
}

void SelfDeletingRequestDelegate::TakeViewerHandle(
    scoped_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = viewer_handle.Pass();
}

// Start loading the viewer URL of the current page in |web_contents|.
void StartNavigationToDistillerViewer(content::WebContents* web_contents,
                                      const GURL& url) {
  GURL viewer_url = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      chrome::kDomDistillerScheme, url);
  content::NavigationController::LoadURLParams params(viewer_url);
  params.transition_type = content::PAGE_TRANSITION_AUTO_BOOKMARK;
  web_contents->GetController().LoadURLWithParams(params);
}

void StartDistillation(content::WebContents* web_contents) {
  // Start distillation using |web_contents|, and ensure ViewerHandle stays
  // around until the viewer requests distillation.
  SelfDeletingRequestDelegate* view_request_delegate =
      new SelfDeletingRequestDelegate(web_contents);
  scoped_ptr<content::WebContents> old_web_contents_sptr(web_contents);
  scoped_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(old_web_contents_sptr.Pass()));
  DomDistillerService* dom_distiller_service =
      DomDistillerServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  scoped_ptr<DistillerPage> distiller_page =
      dom_distiller_service->CreateDefaultDistillerPageWithHandle(
                                 source_page_handle.PassAs<SourcePageHandle>())
          .Pass();

  const GURL& last_committed_url = web_contents->GetLastCommittedURL();
  scoped_ptr<ViewerHandle> viewer_handle = dom_distiller_service->ViewUrl(
      view_request_delegate, distiller_page.Pass(), last_committed_url);
  view_request_delegate->TakeViewerHandle(viewer_handle.Pass());
}

}  // namespace

void DistillCurrentPageAndView(content::WebContents* old_web_contents) {
  DCHECK(old_web_contents);
  // Create new WebContents.
  content::WebContents::CreateParams create_params(
      old_web_contents->GetBrowserContext());
  content::WebContents* new_web_contents =
      content::WebContents::Create(create_params);
  DCHECK(new_web_contents);

  // Copy all navigation state from the old WebContents to the new one.
  new_web_contents->GetController().CopyStateFrom(
      old_web_contents->GetController());

  CoreTabHelper::FromWebContents(old_web_contents)->delegate()->SwapTabContents(
      old_web_contents, new_web_contents, false, false);

  StartNavigationToDistillerViewer(new_web_contents,
                                   old_web_contents->GetLastCommittedURL());
  StartDistillation(old_web_contents);
}
