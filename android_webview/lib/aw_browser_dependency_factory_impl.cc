// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_browser_dependency_factory_impl.h"

// TODO(joth): Componentize or remove chrome/... dependencies.
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/native/android_protocol_handler.h"
#include "android_webview/native/aw_contents_container.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

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

AwBrowserDependencyFactoryImpl::AwBrowserDependencyFactoryImpl()
    : context_dependent_hooks_initialized_(false) {
}

AwBrowserDependencyFactoryImpl::~AwBrowserDependencyFactoryImpl() {}

// static
void AwBrowserDependencyFactoryImpl::InstallInstance() {
  SetInstance(g_lazy_instance.Pointer());
}

// Initializing the Network Delegate here is only a temporary solution until we
// build an Android WebView specific BrowserContext that can handle building
// this internally.
void AwBrowserDependencyFactoryImpl::InitializeNetworkDelegateOnIOThread(
    net::URLRequestContextGetter* normal_context,
    net::URLRequestContextGetter* incognito_context) {
  network_delegate_.reset(new AwNetworkDelegate());
  normal_context->GetURLRequestContext()->set_network_delegate(
      network_delegate_.get());
  incognito_context->GetURLRequestContext()->set_network_delegate(
      network_delegate_.get());
}

void AwBrowserDependencyFactoryImpl::EnsureContextDependentHooksInitialized()
{
  if (context_dependent_hooks_initialized_)
    return;
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  context_dependent_hooks_initialized_ = true;
  profile->GetRequestContext()->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &AwBrowserDependencyFactoryImpl::InitializeNetworkDelegateOnIOThread,
          base::Unretained(this),
          make_scoped_refptr(profile->GetRequestContext()),
          make_scoped_refptr(
              profile->GetOffTheRecordProfile()->GetRequestContext())));

  net::URLRequestContextGetter* context_getter = profile->GetRequestContext();
  AndroidProtocolHandler::RegisterProtocols(context_getter);
}

content::BrowserContext* AwBrowserDependencyFactoryImpl::GetBrowserContext(
    bool incognito) {
  EnsureContextDependentHooksInitialized();
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  return incognito ? profile->GetOffTheRecordProfile() : profile;
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
