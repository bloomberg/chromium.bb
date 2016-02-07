// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/core_services_application_delegate.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "components/clipboard/clipboard_application_delegate.h"
#include "components/filesystem/file_system_app.h"
#include "components/web_view/web_view_application_delegate.h"
#include "mandoline/services/core_services/application_delegate_factory.h"
#include "mojo/logging/init_logging.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/services/tracing/tracing_app.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "url/gurl.h"

namespace core_services {

// A helper class for hosting a mojo::ApplicationImpl on its own thread.
class ApplicationThread : public base::SimpleThread {
 public:
  ApplicationThread(
      const base::WeakPtr<CoreServicesApplicationDelegate>
          core_services_application,
      const std::string& url,
      scoped_ptr<mojo::ApplicationDelegate> delegate,
      mojo::InterfaceRequest<mojo::shell::mojom::Application> request,
      const mojo::Callback<void()>& destruct_callback)
      : base::SimpleThread(url),
        core_services_application_(core_services_application),
        core_services_application_task_runner_(base::MessageLoop::current()
                                                   ->task_runner()),
        url_(url),
        delegate_(std::move(delegate)),
        request_(std::move(request)),
        destruct_callback_(destruct_callback) {}

  ~ApplicationThread() override {
    Join();
    destruct_callback_.Run();
  }

  // Overridden from base::SimpleThread:
  void Run() override {
    scoped_ptr<mojo::ApplicationRunner> runner(
        new mojo::ApplicationRunner(delegate_.release()));
    if (url_ == "mojo://network_service/") {
      runner->set_message_loop_type(base::MessageLoop::TYPE_IO);
    } else if (url_ == "mojo://mus/") {
      runner->set_message_loop_type(base::MessageLoop::TYPE_UI);
    }
    runner->Run(request_.PassMessagePipe().release().value(), false);

    core_services_application_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CoreServicesApplicationDelegate::ApplicationThreadDestroyed,
                   core_services_application_,
                   this));
  }

 private:
  base::WeakPtr<CoreServicesApplicationDelegate> core_services_application_;
  scoped_refptr<base::SingleThreadTaskRunner>
      core_services_application_task_runner_;
  std::string url_;
  scoped_ptr<mojo::ApplicationDelegate> delegate_;
  mojo::InterfaceRequest<mojo::shell::mojom::Application> request_;
  mojo::Callback<void()> destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationThread);
};

CoreServicesApplicationDelegate::CoreServicesApplicationDelegate()
    : weak_factory_(this) {
}

CoreServicesApplicationDelegate::~CoreServicesApplicationDelegate() {
  application_threads_.clear();
}

void CoreServicesApplicationDelegate::ApplicationThreadDestroyed(
    ApplicationThread* thread) {
  ScopedVector<ApplicationThread>::iterator iter =
      std::find(application_threads_.begin(),
                application_threads_.end(),
                thread);
  DCHECK(iter != application_threads_.end());
  application_threads_.erase(iter);
}

void CoreServicesApplicationDelegate::Initialize(mojo::Shell* shell,
                                                 const std::string& url,
                                                 uint32_t id) {
  mojo::InitLogging();
  tracing_.Initialize(shell, url);
}

bool CoreServicesApplicationDelegate::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void CoreServicesApplicationDelegate::Quit() {
  // This will delete all threads. This also performs a blocking join, waiting
  // for the threads to end.
  application_threads_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void CoreServicesApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler> request) {
  handler_bindings_.AddBinding(this, std::move(request));
}

void CoreServicesApplicationDelegate::StartApplication(
    mojo::InterfaceRequest<mojo::shell::mojom::Application> request,
    mojo::URLResponsePtr response,
    const mojo::Callback<void()>& destruct_callback) {
  const std::string url = response->url;

  scoped_ptr<mojo::ApplicationDelegate> delegate;
  if (url == "mojo://clipboard/") {
    delegate.reset(new clipboard::ClipboardApplicationDelegate);
  } else if (url == "mojo://filesystem/") {
    delegate.reset(new filesystem::FileSystemApp);
  } else if (url == "mojo://tracing/") {
    delegate.reset(new tracing::TracingApp);
  } else if (url == "mojo://web_view/") {
    delegate.reset(new web_view::WebViewApplicationDelegate);
  } else {
#if !defined(OS_ANDROID)
    if (!delegate)
      delegate = CreateApplicationDelegateNotAndroid(url);
#endif
    if (!delegate)
      delegate = CreatePlatformSpecificApplicationDelegate(url);

    if (!delegate)
      NOTREACHED() << "This application package does not support " << url;
  }

  scoped_ptr<ApplicationThread> thread(new ApplicationThread(
      weak_factory_.GetWeakPtr(), url, std::move(delegate), std::move(request),
      destruct_callback));
  thread->Start();
  application_threads_.push_back(std::move(thread));
}

}  // namespace core_services
