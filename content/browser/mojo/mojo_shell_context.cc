// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_shell_context.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/process_control.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_registry.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/static_application_loader.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"

#if defined(ENABLE_MEDIA_MOJO_RENDERER)
#include "media/mojo/services/mojo_media_application.h"
#endif

namespace content {

namespace {

// An extra set of apps to register on initialization, if set by a test.
const MojoShellContext::StaticApplicationMap* g_applications_for_test;

void StartProcessOnIOThread(mojo::InterfaceRequest<ProcessControl> request) {
  UtilityProcessHost* process_host =
      UtilityProcessHost::Create(nullptr, nullptr);
  // TODO(rockot): Make it possible for the embedder to associate app URLs with
  // app names so we can have more meaningful process names here.
  process_host->SetName(base::UTF8ToUTF16("Mojo Application"));
  process_host->StartMojoMode();
  ServiceRegistry* services = process_host->GetServiceRegistry();
  services->ConnectToRemoteService(request.Pass());
}

void OnApplicationLoaded(const GURL& url, bool success) {
  if (!success)
    LOG(ERROR) << "Failed to launch Mojo application for " << url.spec();
}

// The default loader to use for all applications. This launches a utility
// process and forwards the Load request the ProcessControl service there.
class UtilityProcessLoader : public mojo::shell::ApplicationLoader {
 public:
  UtilityProcessLoader() {}
  ~UtilityProcessLoader() override {}

 private:
  // mojo::shell::ApplicationLoader:
  void Load(
      const GURL& url,
      mojo::InterfaceRequest<mojo::Application> application_request) override {
    ProcessControlPtr process_control;
    auto process_request = mojo::GetProxy(&process_control);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&StartProcessOnIOThread, base::Passed(&process_request)));
    process_control->LoadApplication(url.spec(), application_request.Pass(),
                                     base::Bind(&OnApplicationLoaded, url));
  }

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessLoader);
};

}  // namespace

// Thread-safe proxy providing access to the shell context from any thread.
class MojoShellContext::Proxy {
 public:
  Proxy(MojoShellContext* shell_context)
      : shell_context_(shell_context),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  ~Proxy() {}

  void ConnectToApplication(
      const GURL& url,
      const GURL& requestor_url,
      mojo::InterfaceRequest<mojo::ServiceProvider> request) {
    if (task_runner_ == base::ThreadTaskRunnerHandle::Get()) {
      if (shell_context_)
        shell_context_->ConnectToApplicationOnOwnThread(url, requestor_url,
                                                        request.Pass());
    } else {
      // |shell_context_| outlives the main MessageLoop, so it's safe for it to
      // be unretained here.
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MojoShellContext::ConnectToApplicationOnOwnThread,
                     base::Unretained(shell_context_), url, requestor_url,
                     base::Passed(&request)));
    }
  }

 private:
  MojoShellContext* shell_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Proxy);
};

// static
base::LazyInstance<scoped_ptr<MojoShellContext::Proxy>>
    MojoShellContext::proxy_ = LAZY_INSTANCE_INITIALIZER;

void MojoShellContext::SetApplicationsForTest(
    const StaticApplicationMap* apps) {
  g_applications_for_test = apps;
}

MojoShellContext::MojoShellContext()
    : application_manager_(new mojo::shell::ApplicationManager(this)) {
  proxy_.Get().reset(new Proxy(this));
  application_manager_->set_default_loader(
      scoped_ptr<mojo::shell::ApplicationLoader>(new UtilityProcessLoader));

  StaticApplicationMap apps;
  GetContentClient()->browser()->RegisterMojoApplications(&apps);
  if (g_applications_for_test) {
    // Add testing apps to the map, potentially overwriting whatever the
    // browser client registered.
    for (const auto& entry : *g_applications_for_test)
      apps[entry.first] = entry.second;
  }
  for (const auto& entry : apps) {
    application_manager_->SetLoaderForURL(
        scoped_ptr<mojo::shell::ApplicationLoader>(
            new mojo::shell::StaticApplicationLoader(entry.second)),
        entry.first);
  }

#if defined(ENABLE_MEDIA_MOJO_RENDERER)
  application_manager_->SetLoaderForURL(
      scoped_ptr<mojo::shell::ApplicationLoader>(
          new mojo::shell::StaticApplicationLoader(
              base::Bind(&media::MojoMediaApplication::CreateApp))),
      media::MojoMediaApplication::AppUrl());
#endif
}

MojoShellContext::~MojoShellContext() {
}

// static
void MojoShellContext::ConnectToApplication(
    const GURL& url,
    const GURL& requestor_url,
    mojo::InterfaceRequest<mojo::ServiceProvider> request) {
  proxy_.Get()->ConnectToApplication(url, requestor_url, request.Pass());
}

void MojoShellContext::ConnectToApplicationOnOwnThread(
    const GURL& url,
    const GURL& requestor_url,
    mojo::InterfaceRequest<mojo::ServiceProvider> request) {
  mojo::URLRequestPtr url_request = mojo::URLRequest::New();
  url_request->url = mojo::String::From(url);
  application_manager_->ConnectToApplication(
      url_request.Pass(), requestor_url, request.Pass(),
      mojo::ServiceProviderPtr(), base::Bind(&base::DoNothing));
}

GURL MojoShellContext::ResolveMappings(const GURL& url) {
  return url;
}

GURL MojoShellContext::ResolveMojoURL(const GURL& url) {
  return url;
}

bool MojoShellContext::CreateFetcher(
    const GURL& url,
    const mojo::shell::Fetcher::FetchCallback& loader_callback) {
  return false;
}

}  // namespace content
