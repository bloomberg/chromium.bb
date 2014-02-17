// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_PAGE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_PAGE_DISTILLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistillerImpl;

struct DistilledPageInfo {
  std::string title;
  std::string html;
  std::string next_page_url;
  std::string prev_page_url;
  std::vector<std::string> image_urls;
  DistilledPageInfo();
  ~DistilledPageInfo();

 private:
  DISALLOW_COPY_AND_ASSIGN(DistilledPageInfo);
};

// Distills a single page of an article.
class PageDistiller : public DistillerPage::Delegate {
 public:
  typedef base::Callback<void(scoped_ptr<DistilledPageInfo> distilled_page,
                              bool distillation_successful)>
      PageDistillerCallback;
  explicit PageDistiller(const DistillerPageFactory& distiller_page_factory);
  virtual ~PageDistiller();

  // Creates an execution context. This must be called once before any calls are
  // made to distill the page.
  virtual void Init();

  // Distills the |url| and posts the |callback| with results.
  virtual void DistillPage(const GURL& url,
                           const PageDistillerCallback& callback);

  // DistillerPage::Delegate
  virtual void OnLoadURLDone() OVERRIDE;
  virtual void OnExecuteJavaScriptDone(const GURL& page_url,
                                       const base::Value* value) OVERRIDE;

 private:
  virtual void LoadURL(const GURL& url);

  // Injects JavaScript to distill a loaded page down to its important content,
  // e.g., extracting a news article from its surrounding boilerplate.
  void GetDistilledContent();

  scoped_ptr<DistillerPage> distiller_page_;
  PageDistillerCallback page_distiller_callback_;

  DISALLOW_COPY_AND_ASSIGN(PageDistiller);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_PAGE_DISTILLER_H_
