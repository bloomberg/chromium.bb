// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/client/web_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace devtools_bridge {

namespace {

class WebClientImpl : public WebClient, private content::WebContentsDelegate {
 public:
  WebClientImpl(content::BrowserContext* context, Delegate* delegate);

 private:
  Delegate* const delegate_;
  const scoped_ptr<content::WebContents> web_contents_;
};

WebClientImpl::WebClientImpl(content::BrowserContext* context,
                             Delegate* delegate)
    : delegate_(delegate),
      web_contents_(content::WebContents::Create(
          content::WebContents::CreateParams(context))) {
  web_contents_->SetDelegate(this);
}

}  // namespace

scoped_ptr<WebClient> WebClient::CreateInstance(
    content::BrowserContext* context, Delegate* delegate) {
  return make_scoped_ptr(new WebClientImpl(context, delegate));
}

}  // namespace devtools_bridge
