// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// Maximum number of distilled pages in an article.
const int kMaxPagesInArticle = 32;
}

namespace dom_distiller {

DistillerFactoryImpl::DistillerFactoryImpl(
    scoped_ptr<DistillerPageFactory> distiller_page_factory,
    scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory)
  : distiller_page_factory_(distiller_page_factory.Pass()),
    distiller_url_fetcher_factory_(distiller_url_fetcher_factory.Pass()) {}

DistillerFactoryImpl::~DistillerFactoryImpl() {}

scoped_ptr<Distiller> DistillerFactoryImpl::CreateDistiller() {
  scoped_ptr<DistillerImpl> distiller(new DistillerImpl(
      *distiller_page_factory_, *distiller_url_fetcher_factory_));
  distiller->Init();
  return distiller.PassAs<Distiller>();
}

DistillerImpl::DistillerImpl(
    const DistillerPageFactory& distiller_page_factory,
    const DistillerURLFetcherFactory& distiller_url_fetcher_factory)
    : distiller_url_fetcher_factory_(distiller_url_fetcher_factory),
      distillation_in_progress_(false) {
  page_distiller_.reset(new PageDistiller(distiller_page_factory));
}

DistillerImpl::~DistillerImpl() {
}

void DistillerImpl::Init() {
  DCHECK(!distillation_in_progress_);
  page_distiller_->Init();
  article_proto_.reset(new DistilledArticleProto());
}

void DistillerImpl::DistillPage(const GURL& url,
                            const DistillerCallback& distillation_cb) {
  DCHECK(!distillation_in_progress_);
  distillation_cb_ = distillation_cb;
  DistillPage(url);
}

void DistillerImpl::DistillPage(const GURL& url) {
  DCHECK(!distillation_in_progress_);
  if (url.is_valid() && article_proto_->pages_size() < kMaxPagesInArticle &&
      processed_urls_.find(url.spec()) == processed_urls_.end()) {
    distillation_in_progress_ = true;
    // Distill the next page.
    DCHECK(url.is_valid());
    DCHECK_LT(article_proto_->pages_size(), kMaxPagesInArticle);
    page_distiller_->DistillPage(
        url,
        base::Bind(&DistillerImpl::OnPageDistillationFinished,
                   base::Unretained(this),
                   url));
  } else {
    RunDistillerCallbackIfDone();
  }
}

void DistillerImpl::OnPageDistillationFinished(
    const GURL& page_url,
    scoped_ptr<DistilledPageInfo> distilled_page,
    bool distillation_successful) {
  DCHECK(distillation_in_progress_);
  DCHECK(distilled_page.get());
  if (!distillation_successful) {
    RunDistillerCallbackIfDone();
  } else {
    DistilledPageProto* current_page = article_proto_->add_pages();
    // Set the title of the article as the title of the first page.
    if (article_proto_->pages_size() == 1) {
      article_proto_->set_title(distilled_page->title);
    }

    current_page->set_url(page_url.spec());
    current_page->set_html(distilled_page->html);

    GURL next_page_url(distilled_page->next_page_url);
    if (next_page_url.is_valid()) {
      // The pages should be in same origin.
      DCHECK_EQ(next_page_url.GetOrigin(), page_url.GetOrigin());
    }

    processed_urls_.insert(page_url.spec());
    distillation_in_progress_ = false;
    int page_number = article_proto_->pages_size();
    for (size_t img_num = 0; img_num < distilled_page->image_urls.size();
         ++img_num) {
      std::string image_id =
          base::IntToString(page_number) + "_" + base::IntToString(img_num);
      FetchImage(current_page, image_id, distilled_page->image_urls[img_num]);
    }
    DistillPage(next_page_url);
  }
}

void DistillerImpl::FetchImage(DistilledPageProto* distilled_page_proto,
                               const std::string& image_id,
                               const std::string& item) {
  DistillerURLFetcher* fetcher =
      distiller_url_fetcher_factory_.CreateDistillerURLFetcher();
  image_fetchers_[image_id] = fetcher;
  fetcher->FetchURL(item,
                    base::Bind(&DistillerImpl::OnFetchImageDone,
                               base::Unretained(this),
                               base::Unretained(distilled_page_proto),
                               image_id));
}

void DistillerImpl::OnFetchImageDone(DistilledPageProto* distilled_page_proto,
                                     const std::string& id,
                                     const std::string& response) {
  DCHECK_GT(article_proto_->pages_size(), 0);
  DCHECK(distilled_page_proto);
  DistilledPageProto_Image* image = distilled_page_proto->add_image();
  image->set_name(id);
  image->set_data(response);
  DCHECK(image_fetchers_.end() != image_fetchers_.find(id));
  DistillerURLFetcher* fetcher = image_fetchers_[id];
  int result = image_fetchers_.erase(id);
  delete fetcher;
  DCHECK_EQ(1, result);
  RunDistillerCallbackIfDone();
}

void DistillerImpl::RunDistillerCallbackIfDone() {
  if (image_fetchers_.empty() && !distillation_in_progress_) {
    distillation_cb_.Run(article_proto_.Pass());
  }
}

}  // namespace dom_distiller
