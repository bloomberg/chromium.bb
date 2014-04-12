// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLER_PAGE_WEB_CONTENTS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLER_PAGE_WEB_CONTENTS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class RenderViewHost;
}

using content::RenderViewHost;

namespace dom_distiller {

class DistillerContext;

class DistillerPageWebContentsFactory : public DistillerPageFactory {
 public:
  explicit DistillerPageWebContentsFactory(
      content::BrowserContext* browser_context)
      : DistillerPageFactory(), browser_context_(browser_context) {}
  virtual ~DistillerPageWebContentsFactory() {}

  virtual scoped_ptr<DistillerPage> CreateDistillerPage() const OVERRIDE;

 private:
  content::BrowserContext* browser_context_;
};

class DistillerPageWebContents : public DistillerPage,
                                 public content::WebContentsObserver {
 public:
  DistillerPageWebContents(content::BrowserContext* browser_context);
  virtual ~DistillerPageWebContents();

  // content::WebContentsObserver implementation.
  virtual void DocumentLoadedInFrame(int64 frame_id,
                                     RenderViewHost* render_view_host) OVERRIDE;

  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const base::string16& error_description,
                           RenderViewHost* render_view_host) OVERRIDE;

 protected:
  virtual void InitImpl() OVERRIDE;
  virtual void LoadURLImpl(const GURL& gurl) OVERRIDE;
  virtual void ExecuteJavaScriptImpl(const std::string& script) OVERRIDE;

 private:
  scoped_ptr<content::WebContents> web_contents_;
  content::BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(DistillerPageWebContents);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLER_PAGE_WEB_CONTENTS_H_
