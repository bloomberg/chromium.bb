// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_browser_dependency_factory_impl.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "base/lazy_instance.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

using content::BrowserContext;
using content::WebContents;

namespace android_webview {

namespace {

base::LazyInstance<AwBrowserDependencyFactoryImpl>::Leaky g_lazy_instance;

}  // namespace

AwBrowserDependencyFactoryImpl::AwBrowserDependencyFactoryImpl() {}

AwBrowserDependencyFactoryImpl::~AwBrowserDependencyFactoryImpl() {}

// static
void AwBrowserDependencyFactoryImpl::InstallInstance() {
  SetInstance(g_lazy_instance.Pointer());
}

content::BrowserContext* AwBrowserDependencyFactoryImpl::GetBrowserContext() {
  return static_cast<AwContentBrowserClient*>(
      content::GetContentClient()->browser())->GetAwBrowserContext();
}

WebContents* AwBrowserDependencyFactoryImpl::CreateWebContents() {
  return content::WebContents::Create(
      content::WebContents::CreateParams(GetBrowserContext()));
}

}  // namespace android_webview
