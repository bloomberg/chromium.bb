// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "grit/dom_distiller_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

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
  : distiller_page_factory_(distiller_page_factory),
    distiller_url_fetcher_factory_(distiller_url_fetcher_factory) {
  distiller_page_ = distiller_page_factory_.CreateDistillerPage(this).Pass();
}

DistillerImpl::~DistillerImpl() {
}

void DistillerImpl::Init() {
  distiller_page_->Init();
}

void DistillerImpl::DistillPage(const GURL& url,
                            const DistillerCallback& distillation_cb) {
  distillation_cb_ = distillation_cb;
  proto_.reset(new DistilledPageProto());
  proto_->set_url(url.spec());
  LoadURL(url);
}

void DistillerImpl::LoadURL(const GURL& url) {
  distiller_page_->LoadURL(url);
}

void DistillerImpl::OnLoadURLDone() {
  GetDistilledContent();
}

void DistillerImpl::GetDistilledContent() {
  std::string script =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DISTILLER_JS).as_string();
  distiller_page_->ExecuteJavaScript(script);
}

void DistillerImpl::OnExecuteJavaScriptDone(const base::Value* value) {
  std::string result;
  bool fetched_image = false;
  const base::ListValue* result_list = NULL;
  if (!value->GetAsList(&result_list)) {
    DCHECK(proto_);
    distillation_cb_.Run(proto_.Pass());
    return;
  }
  int i = 0;
  for (base::ListValue::const_iterator iter = result_list->begin();
      iter != result_list->end(); ++iter, ++i) {
    std::string item;
    (*iter)->GetAsString(&item);
    // The JavaScript returns an array where the first element is the title,
    // the second element is the article content HTML, and the remaining
    // elements are image URLs referenced in the HTML.
    switch (i) {
      case 0:
        proto_->set_title(item);
        break;
      case 1:
        proto_->set_html(item);
        break;
      default:
        int image_number = i - 2;
        std::string image_id = base::StringPrintf("%d", image_number);
        FetchImage(image_id, item);
        fetched_image = true;
    }
  }
  if (!fetched_image)
    distillation_cb_.Run(proto_.Pass());
}

void DistillerImpl::FetchImage(const std::string& image_id,
                               const std::string& item) {
  DistillerURLFetcher* fetcher =
      distiller_url_fetcher_factory_.CreateDistillerURLFetcher();
  image_fetchers_[image_id] = fetcher;
  fetcher->FetchURL(item,
                    base::Bind(&DistillerImpl::OnFetchImageDone,
                               base::Unretained(this), image_id));
}

void DistillerImpl::OnFetchImageDone(const std::string& id,
                                     const std::string& response) {
  DCHECK(proto_);
  DistilledPageProto_Image* image = proto_->add_image();
  image->set_name(id);
  image->set_data(response);
  DCHECK(image_fetchers_.end() != image_fetchers_.find(id));
  DistillerURLFetcher* fetcher = image_fetchers_[id];
  int result = image_fetchers_.erase(id);
  delete fetcher;
  DCHECK_EQ(1, result);
  if (image_fetchers_.empty()) {
    distillation_cb_.Run(proto_.Pass());
  }
}

}  // namespace dom_distiller
