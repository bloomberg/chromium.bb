// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_browser_dependency_factory_impl.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/native/aw_contents_container.h"
#include "base/lazy_instance.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message.h"

using content::BrowserContext;
using content::WebContents;

namespace android_webview {

namespace {

base::LazyInstance<AwBrowserDependencyFactoryImpl>::Leaky g_lazy_instance;

class WebContentsWrapper : public AwContentsContainer {
 public:
  WebContentsWrapper(WebContents* web_contents)
      : web_contents_(web_contents) {
  }

  virtual ~WebContentsWrapper() {}

  // AwContentsContainer
  virtual content::WebContents* GetWebContents() OVERRIDE {
    return web_contents_.get();
  }

 private:
  scoped_ptr<content::WebContents> web_contents_;
};

}  // namespace

AwBrowserDependencyFactoryImpl::AwBrowserDependencyFactoryImpl() {
}

AwBrowserDependencyFactoryImpl::~AwBrowserDependencyFactoryImpl() {}

// static
void AwBrowserDependencyFactoryImpl::InstallInstance() {
  SetInstance(g_lazy_instance.Pointer());
}

content::BrowserContext* AwBrowserDependencyFactoryImpl::GetBrowserContext(
    bool incognito) {
  // TODO(boliu): Remove incognito parameter.
  LOG_IF(ERROR, incognito) << "Android WebView does not support incognito mode"
                           << " yet. Creating normal profile instead.";
  return static_cast<AwContentBrowserClient*>(
      content::GetContentClient()->browser())->GetAwBrowserContext();
}

WebContents* AwBrowserDependencyFactoryImpl::CreateWebContents(bool incognito) {
  return content::WebContents::Create(
      GetBrowserContext(incognito), 0, MSG_ROUTING_NONE, 0);
}

AwContentsContainer* AwBrowserDependencyFactoryImpl::CreateContentsContainer(
    content::WebContents* contents) {
  return new WebContentsWrapper(contents);
}

}  // namespace android_webview
