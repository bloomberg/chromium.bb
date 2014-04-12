// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "grit/component_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

DistilledPageInfo::DistilledPageInfo() {}

DistilledPageInfo::~DistilledPageInfo() {}

DistillerPageFactory::~DistillerPageFactory() {}

DistillerPage::DistillerPage() : state_(NO_CONTEXT) {}

DistillerPage::~DistillerPage() {}

void DistillerPage::Init() {
  DCHECK_EQ(NO_CONTEXT, state_);
  InitImpl();
  state_ = IDLE;
}

void DistillerPage::DistillPage(const GURL& gurl,
                                const DistillerPageCallback& callback) {
  DCHECK(state_ == IDLE || state_ == PAGE_AVAILABLE ||
         state_ == PAGELOAD_FAILED);
  state_ = LOADING_PAGE;
  distiller_page_callback_ = callback;
  LoadURLImpl(gurl);
}

void DistillerPage::ExecuteJavaScript() {
  DCHECK_EQ(PAGE_AVAILABLE, state_);
  state_ = EXECUTING_JAVASCRIPT;
  DVLOG(1) << "Beginning distillation";
  std::string script = ResourceBundle::GetSharedInstance()
                           .GetRawDataResource(IDR_DISTILLER_JS)
                           .as_string();
  ExecuteJavaScriptImpl(script);
}

void DistillerPage::OnLoadURLDone() {
  DCHECK_EQ(LOADING_PAGE, state_);
  state_ = PAGE_AVAILABLE;
  ExecuteJavaScript();
}

void DistillerPage::OnLoadURLFailed() {
  state_ = PAGELOAD_FAILED;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(distiller_page_callback_,
                 base::Passed(scoped_ptr<DistilledPageInfo>()),
                 false));
}

void DistillerPage::OnExecuteJavaScriptDone(const GURL& page_url,
                                            const base::Value* value) {
  DCHECK_EQ(EXECUTING_JAVASCRIPT, state_);
  state_ = PAGE_AVAILABLE;
  scoped_ptr<DistilledPageInfo> page_info(new DistilledPageInfo());
  std::string result;
  const base::ListValue* result_list = NULL;
  bool found_content = false;
  if (!value->GetAsList(&result_list)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(distiller_page_callback_, base::Passed(&page_info), false));
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
        base::Bind(
            distiller_page_callback_, base::Passed(&page_info), found_content));
  }
}

}  // namespace dom_distiller
