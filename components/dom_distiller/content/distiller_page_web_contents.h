// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLER_PAGE_WEB_CONTENTS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLER_PAGE_WEB_CONTENTS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace dom_distiller {

class SourcePageHandleWebContents : public SourcePageHandle {
 public:
  explicit SourcePageHandleWebContents(
      scoped_ptr<content::WebContents> web_contents);
  virtual ~SourcePageHandleWebContents();

  scoped_ptr<content::WebContents> GetWebContents();

 private:
  // The WebContents this class owns.
  scoped_ptr<content::WebContents> web_contents_;
};

class DistillerPageWebContentsFactory : public DistillerPageFactory {
 public:
  explicit DistillerPageWebContentsFactory(
      content::BrowserContext* browser_context)
      : DistillerPageFactory(), browser_context_(browser_context) {}
  virtual ~DistillerPageWebContentsFactory() {}

  virtual scoped_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& render_view_size) const OVERRIDE;
  virtual scoped_ptr<DistillerPage> CreateDistillerPageWithHandle(
      scoped_ptr<SourcePageHandle> handle) const OVERRIDE;

 private:
  content::BrowserContext* browser_context_;
};

class DistillerPageWebContents : public DistillerPage,
                                 public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  DistillerPageWebContents(
      content::BrowserContext* browser_context,
      const gfx::Size& render_view_size,
      scoped_ptr<SourcePageHandleWebContents> optional_web_contents_handle);
  virtual ~DistillerPageWebContents();

  // content::WebContentsDelegate implementation.
  virtual gfx::Size GetSizeForNewRenderView(
      content::WebContents* web_contents) const OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) OVERRIDE;

  virtual void DidFailLoad(content::RenderFrameHost* render_frame_host,
                           const GURL& validated_url,
                           int error_code,
                           const base::string16& error_description) OVERRIDE;

 protected:
  virtual void DistillPageImpl(const GURL& url,
                               const std::string& script) OVERRIDE;

 private:
  friend class TestDistillerPageWebContents;

  enum State {
    // The page distiller is idle.
    IDLE,
    // A page is currently loading.
    LOADING_PAGE,
    // There was an error processing the page.
    PAGELOAD_FAILED,
    // JavaScript is executing within the context of the page. When the
    // JavaScript completes, the state will be returned to |IDLE|.
    EXECUTING_JAVASCRIPT
  };

  // Creates a new WebContents, adds |this| as an observer, and loads the
  // |url|.
  virtual void CreateNewWebContents(const GURL& url);

  // Injects and executes JavaScript in the context of a loaded page. This
  // must only be called after the page has successfully loaded.
  void ExecuteJavaScript();

  // Called when the distillation is done or if the page load failed.
  void OnWebContentsDistillationDone(const GURL& page_url,
                                     const base::Value* value);

  // The current state of the |DistillerPage|, initially |IDLE|.
  State state_;

  // The JavaScript to inject to extract content.
  std::string script_;

  scoped_ptr<content::WebContents> web_contents_;
  content::BrowserContext* browser_context_;
  gfx::Size render_view_size_;
  DISALLOW_COPY_AND_ASSIGN(DistillerPageWebContents);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLER_PAGE_WEB_CONTENTS_H_
