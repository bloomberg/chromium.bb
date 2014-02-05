// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_

#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/page_distiller.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistillerImpl;

class Distiller {
 public:
  typedef base::Callback<void(scoped_ptr<DistilledArticleProto>)>
      DistillerCallback;
  virtual ~Distiller() {}

  // Distills a page, and asynchrounously returns the article HTML to the
  // supplied callback.
  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) = 0;
};

class DistillerFactory {
 public:
  virtual scoped_ptr<Distiller> CreateDistiller() = 0;
  virtual ~DistillerFactory() {}
};

// Factory for creating a Distiller.
class DistillerFactoryImpl : public DistillerFactory {
 public:
  DistillerFactoryImpl(
      scoped_ptr<DistillerPageFactory> distiller_page_factory,
      scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory);
  virtual ~DistillerFactoryImpl();
  virtual scoped_ptr<Distiller> CreateDistiller() OVERRIDE;

 private:
  scoped_ptr<DistillerPageFactory> distiller_page_factory_;
  scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory_;
};

// Distills a article from a page and associated pages.
class DistillerImpl : public Distiller {
 public:
  DistillerImpl(
      const DistillerPageFactory& distiller_page_factory,
      const DistillerURLFetcherFactory& distiller_url_fetcher_factory);
  virtual ~DistillerImpl();

  // Creates an execution context. This must be called once before any calls are
  // made to distill the page.
  virtual void Init();

  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) OVERRIDE;

 private:
  void OnFetchImageDone(DistilledPageProto* distilled_page_proto,
                        const std::string& id,
                        const std::string& response);

  void OnPageDistillationFinished(const GURL& page_url,
                                  scoped_ptr<DistilledPageInfo> distilled_page,
                                  bool distillation_successful);

  virtual void FetchImage(DistilledPageProto* distilled_page_proto,
                          const std::string& image_id,
                          const std::string& item);

  // Distills the page and adds the new page to |article_proto|.
  void DistillPage(const GURL& url);

  // Runs |distillation_cb_| if all distillation callbacks and image fetches are
  // complete.
  void RunDistillerCallbackIfDone();

  const DistillerURLFetcherFactory& distiller_url_fetcher_factory_;
  scoped_ptr<PageDistiller> page_distiller_;
  DistillerCallback distillation_cb_;

  base::hash_map<std::string, DistillerURLFetcher*> image_fetchers_;
  scoped_ptr<DistilledArticleProto> article_proto_;
  bool distillation_in_progress_;
  // Set to keep track of which urls are already seen by the distiller.
  base::hash_set<std::string> processed_urls_;

  DISALLOW_COPY_AND_ASSIGN(DistillerImpl);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
