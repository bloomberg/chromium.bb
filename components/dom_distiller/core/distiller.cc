// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <map>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
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
const size_t kMaxPagesInArticle = 32;
}

namespace dom_distiller {

DistillerFactoryImpl::DistillerFactoryImpl(
    scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory,
    const dom_distiller::proto::DomDistillerOptions& dom_distiller_options)
    : distiller_url_fetcher_factory_(distiller_url_fetcher_factory.Pass()),
      dom_distiller_options_(dom_distiller_options) {
}

DistillerFactoryImpl::~DistillerFactoryImpl() {}

scoped_ptr<Distiller> DistillerFactoryImpl::CreateDistiller() {
  scoped_ptr<DistillerImpl> distiller(new DistillerImpl(
      *distiller_url_fetcher_factory_, dom_distiller_options_));
  return distiller.PassAs<Distiller>();
}

DistillerImpl::DistilledPageData::DistilledPageData() {}

DistillerImpl::DistilledPageData::~DistilledPageData() {}

DistillerImpl::DistillerImpl(
    const DistillerURLFetcherFactory& distiller_url_fetcher_factory,
    const dom_distiller::proto::DomDistillerOptions& dom_distiller_options)
    : distiller_url_fetcher_factory_(distiller_url_fetcher_factory),
      dom_distiller_options_(dom_distiller_options),
      max_pages_in_article_(kMaxPagesInArticle),
      destruction_allowed_(true),
      weak_factory_(this) {
}

DistillerImpl::~DistillerImpl() {
  DCHECK(destruction_allowed_);
}

void DistillerImpl::SetMaxNumPagesInArticle(size_t max_num_pages) {
  max_pages_in_article_ = max_num_pages;
}

bool DistillerImpl::AreAllPagesFinished() const {
  return started_pages_index_.empty() && waiting_pages_.empty();
}

size_t DistillerImpl::TotalPageCount() const {
  return waiting_pages_.size() + started_pages_index_.size() +
         finished_pages_index_.size();
}

void DistillerImpl::AddToDistillationQueue(int page_num, const GURL& url) {
  if (!IsPageNumberInUse(page_num) && url.is_valid() &&
      TotalPageCount() < max_pages_in_article_ &&
      seen_urls_.find(url.spec()) == seen_urls_.end()) {
    waiting_pages_[page_num] = url;
  }
}

bool DistillerImpl::IsPageNumberInUse(int page_num) const {
  return waiting_pages_.find(page_num) != waiting_pages_.end() ||
         started_pages_index_.find(page_num) != started_pages_index_.end() ||
         finished_pages_index_.find(page_num) != finished_pages_index_.end();
}

DistillerImpl::DistilledPageData* DistillerImpl::GetPageAtIndex(size_t index)
    const {
  DCHECK_LT(index, pages_.size());
  DistilledPageData* page_data = pages_[index];
  DCHECK(page_data);
  return page_data;
}

void DistillerImpl::DistillPage(const GURL& url,
                                scoped_ptr<DistillerPage> distiller_page,
                                const DistillationFinishedCallback& finished_cb,
                                const DistillationUpdateCallback& update_cb) {
  DCHECK(AreAllPagesFinished());
  distiller_page_ = distiller_page.Pass();
  finished_cb_ = finished_cb;
  update_cb_ = update_cb;

  AddToDistillationQueue(0, url);
  DistillNextPage();
}

void DistillerImpl::DistillNextPage() {
  if (!waiting_pages_.empty()) {
    std::map<int, GURL>::iterator front = waiting_pages_.begin();
    int page_num = front->first;
    const GURL url = front->second;

    waiting_pages_.erase(front);
    DCHECK(url.is_valid());
    DCHECK(started_pages_index_.find(page_num) == started_pages_index_.end());
    DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
    seen_urls_.insert(url.spec());
    pages_.push_back(new DistilledPageData());
    started_pages_index_[page_num] = pages_.size() - 1;
    distiller_page_->DistillPage(
        url,
        dom_distiller_options_,
        base::Bind(&DistillerImpl::OnPageDistillationFinished,
                   weak_factory_.GetWeakPtr(),
                   page_num,
                   url));
  }
}

void DistillerImpl::OnPageDistillationFinished(
    int page_num,
    const GURL& page_url,
    scoped_ptr<proto::DomDistillerResult> distiller_result,
    bool distillation_successful) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  if (distillation_successful) {
    DCHECK(distiller_result.get());
    DistilledPageData* page_data =
        GetPageAtIndex(started_pages_index_[page_num]);
    page_data->distilled_page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_data->page_num = page_num;
    if (distiller_result->has_title()) {
      page_data->distilled_page_proto->data.set_title(
          distiller_result->title());
    }
    page_data->distilled_page_proto->data.set_url(page_url.spec());
    if (distiller_result->has_distilled_content() &&
        distiller_result->distilled_content().has_html()) {
      page_data->distilled_page_proto->data.set_html(
          distiller_result->distilled_content().html());
    }
    if (distiller_result->has_debug_info() &&
        distiller_result->debug_info().has_log()) {
      page_data->distilled_page_proto->data.mutable_debug_info()->set_log(
          distiller_result->debug_info().log());
    }

    if (distiller_result->has_pagination_info()) {
      proto::PaginationInfo pagination_info =
          distiller_result->pagination_info();
      if (pagination_info.has_next_page()) {
        GURL next_page_url(pagination_info.next_page());
        if (next_page_url.is_valid()) {
          // The pages should be in same origin.
          DCHECK_EQ(next_page_url.GetOrigin(), page_url.GetOrigin());
          AddToDistillationQueue(page_num + 1, next_page_url);
        }
      }

      if (pagination_info.has_prev_page()) {
        GURL prev_page_url(pagination_info.prev_page());
        if (prev_page_url.is_valid()) {
          DCHECK_EQ(prev_page_url.GetOrigin(), page_url.GetOrigin());
          AddToDistillationQueue(page_num - 1, prev_page_url);
        }
      }
    }

    for (int img_num = 0; img_num < distiller_result->image_urls_size();
         ++img_num) {
      std::string image_id =
          base::IntToString(page_num + 1) + "_" + base::IntToString(img_num);
      FetchImage(page_num, image_id, distiller_result->image_urls(img_num));
    }

    AddPageIfDone(page_num);
    DistillNextPage();
  } else {
    started_pages_index_.erase(page_num);
    RunDistillerCallbackIfDone();
  }
}

void DistillerImpl::FetchImage(int page_num,
                               const std::string& image_id,
                               const std::string& item) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  DistillerURLFetcher* fetcher =
      distiller_url_fetcher_factory_.CreateDistillerURLFetcher();
  page_data->image_fetchers_.push_back(fetcher);

  fetcher->FetchURL(item,
                    base::Bind(&DistillerImpl::OnFetchImageDone,
                               weak_factory_.GetWeakPtr(),
                               page_num,
                               base::Unretained(fetcher),
                               image_id));
}

void DistillerImpl::OnFetchImageDone(int page_num,
                                     DistillerURLFetcher* url_fetcher,
                                     const std::string& id,
                                     const std::string& response) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  DCHECK(page_data->distilled_page_proto.get());
  DCHECK(url_fetcher);
  ScopedVector<DistillerURLFetcher>::iterator fetcher_it =
      std::find(page_data->image_fetchers_.begin(),
                page_data->image_fetchers_.end(),
                url_fetcher);

  DCHECK(fetcher_it != page_data->image_fetchers_.end());
  // Delete the |url_fetcher| by DeleteSoon since the OnFetchImageDone
  // callback is invoked by the |url_fetcher|.
  page_data->image_fetchers_.weak_erase(fetcher_it);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, url_fetcher);

  DistilledPageProto_Image* image =
      page_data->distilled_page_proto->data.add_image();
  image->set_name(id);
  image->set_data(response);

  AddPageIfDone(page_num);
}

void DistillerImpl::AddPageIfDone(int page_num) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  if (page_data->image_fetchers_.empty()) {
    finished_pages_index_[page_num] = started_pages_index_[page_num];
    started_pages_index_.erase(page_num);
    const ArticleDistillationUpdate& article_update =
        CreateDistillationUpdate();
    DCHECK_EQ(article_update.GetPagesSize(), finished_pages_index_.size());
    update_cb_.Run(article_update);
    RunDistillerCallbackIfDone();
  }
}

const ArticleDistillationUpdate DistillerImpl::CreateDistillationUpdate()
    const {
  bool has_prev_page = false;
  bool has_next_page = false;
  if (!finished_pages_index_.empty()) {
    int prev_page_num = finished_pages_index_.begin()->first - 1;
    int next_page_num = finished_pages_index_.rbegin()->first + 1;
    has_prev_page = IsPageNumberInUse(prev_page_num);
    has_next_page = IsPageNumberInUse(next_page_num);
  }

  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto> >
      update_pages;
  for (std::map<int, size_t>::const_iterator it = finished_pages_index_.begin();
       it != finished_pages_index_.end();
       ++it) {
    update_pages.push_back(pages_[it->second]->distilled_page_proto);
  }
  return ArticleDistillationUpdate(update_pages, has_next_page, has_prev_page);
}

void DistillerImpl::RunDistillerCallbackIfDone() {
  DCHECK(!finished_cb_.is_null());
  if (AreAllPagesFinished()) {
    bool first_page = true;
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    // Stitch the pages back into the article.
    for (std::map<int, size_t>::iterator it = finished_pages_index_.begin();
         it != finished_pages_index_.end();) {
      DistilledPageData* page_data = GetPageAtIndex(it->second);
      *(article_proto->add_pages()) = page_data->distilled_page_proto->data;

      if (first_page) {
        article_proto->set_title(page_data->distilled_page_proto->data.title());
        first_page = false;
      }

      finished_pages_index_.erase(it++);
    }

    pages_.clear();
    DCHECK_LE(static_cast<size_t>(article_proto->pages_size()),
              max_pages_in_article_);

    DCHECK(pages_.empty());
    DCHECK(finished_pages_index_.empty());

    base::AutoReset<bool> dont_delete_this_in_callback(&destruction_allowed_,
                                                       false);
    finished_cb_.Run(article_proto.Pass());
    finished_cb_.Reset();
  }
}

}  // namespace dom_distiller
