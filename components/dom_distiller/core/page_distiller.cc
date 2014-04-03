// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/page_distiller.h"

#include <map>

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "grit/component_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

DistilledPageInfo::DistilledPageInfo() {}

DistilledPageInfo::~DistilledPageInfo() {}

PageDistiller::PageDistiller(const DistillerPageFactory& distiller_page_factory)
    : weak_factory_(this) {
  distiller_page_ =
      distiller_page_factory.CreateDistillerPage(weak_factory_.GetWeakPtr())
          .Pass();
}

PageDistiller::~PageDistiller() {}

void PageDistiller::Init() { distiller_page_->Init(); }

void PageDistiller::DistillPage(const GURL& url,
                                const PageDistillerCallback& callback) {
  page_distiller_callback_ = callback;
  LoadURL(url);
}

void PageDistiller::LoadURL(const GURL& url) {
  DVLOG(1) << "Loading for distillation: " << url.spec();
  distiller_page_->LoadURL(url);
}

void PageDistiller::OnLoadURLDone() { GetDistilledContent(); }

void PageDistiller::GetDistilledContent() {
  DVLOG(1) << "Beginning distillation";
  std::string script = ResourceBundle::GetSharedInstance()
                           .GetRawDataResource(IDR_DISTILLER_JS)
                           .as_string();
  distiller_page_->ExecuteJavaScript(script);
}

void PageDistiller::OnExecuteJavaScriptDone(const GURL& page_url,
                                            const base::Value* value) {
  DVLOG(1) << "Distillation complete; extracting resources for "
      << page_url.spec();

  scoped_ptr<DistilledPageInfo> page_info(new DistilledPageInfo());
  std::string result;
  const base::ListValue* result_list = NULL;
  bool found_content = false;
  if (!value->GetAsList(&result_list)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(page_distiller_callback_, base::Passed(&page_info), false));
  } else {
    int i = 0;
    for (base::ListValue::const_iterator iter = result_list->begin();
         iter != result_list->end();
         ++iter, ++i) {
      std::string item;
      (*iter)->GetAsString(&item);
      // The JavaScript returns an array where the first element is the title,
      // the second element is the article content HTML, and the remaining
      // elements are image URLs referenced in the HTML.
      switch (i) {
        case 0:
          page_info->title = item;
          break;
        case 1:
          page_info->html = item;
          found_content = true;
          break;
        case 2:
          page_info->next_page_url = item;
          break;
        case 3:
          page_info->prev_page_url = item;
          break;
        default:
          if (GURL(item).is_valid()) {
            page_info->image_urls.push_back(item);
          }
      }
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(page_distiller_callback_,
                   base::Passed(&page_info),
                   found_content));
  }
}

}  // namespace dom_distiller
