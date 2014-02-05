// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include "base/logging.h"
#include "url/gurl.h"

namespace dom_distiller {

DistillerPageFactory::~DistillerPageFactory() {}

DistillerPage::DistillerPage(
    DistillerPage::Delegate* delegate)
    : state_(NO_CONTEXT), delegate_(delegate) {
}

DistillerPage::~DistillerPage() {
}

void DistillerPage::Init() {
  DCHECK_EQ(NO_CONTEXT, state_);
  InitImpl();
  state_ = IDLE;
}

void DistillerPage::LoadURL(const GURL& gurl) {
  DCHECK(state_ == IDLE ||
         state_ == PAGE_AVAILABLE ||
         state_ == PAGELOAD_FAILED);
  state_ = LOADING_PAGE;
  LoadURLImpl(gurl);
}

void DistillerPage::ExecuteJavaScript(const std::string& script) {
  DCHECK_EQ(PAGE_AVAILABLE, state_);
  state_ = EXECUTING_JAVASCRIPT;
  ExecuteJavaScriptImpl(script);
}

void DistillerPage::OnLoadURLDone() {
  DCHECK_EQ(LOADING_PAGE, state_);
  state_ = PAGE_AVAILABLE;
  if (!delegate_)
    return;
  delegate_->OnLoadURLDone();
}

void DistillerPage::OnLoadURLFailed() {
  state_ = PAGELOAD_FAILED;
  scoped_ptr<base::Value> empty(base::Value::CreateNullValue());
  if (!delegate_)
    return;
  delegate_->OnExecuteJavaScriptDone(GURL(), empty.get());
}

void DistillerPage::OnExecuteJavaScriptDone(const GURL& page_url,
                                            const base::Value* value) {
  DCHECK_EQ(EXECUTING_JAVASCRIPT, state_);
  state_ = PAGE_AVAILABLE;
  if (!delegate_)
    return;
  delegate_->OnExecuteJavaScriptDone(page_url, value);
}

}  // namespace dom_distiller
