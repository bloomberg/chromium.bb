// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/core_services_application_delegate.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "components/clipboard/clipboard_application_delegate.h"
#include "components/native_viewport/native_viewport_application_delegate.h"
#include "components/resource_provider/resource_provider_app.h"
#include "components/surfaces/surfaces_service_application.h"
#include "components/view_manager/view_manager_app.h"
#include "mandoline/ui/browser/browser.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/services/network/network_service_delegate.h"
#include "mojo/services/tracing/tracing_app.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "mandoline/ui/omnibox/omnibox_impl.h"
#endif

namespace core_services {

class ApplicationThread;

// A base::Thread which hosts a mojo::ApplicationImpl on its own thread.
//
// Why not base::SimpleThread? The comments in SimpleThread discourage its use,
// and we want most of what base::Thread provides. The hack where we call
// SetThreadWasQuitProperly() in Run() is already used in the chrome codebase.
// (By the time we building a base::Thread here, we already have a MessageLoop
// on our thread, along with a lot of other bookkeeping objects, too. This is
// why we don't call into ApplicationRunnerChromium; we already have an
// AtExitManager et all at this point.)
class ApplicationThread : public base::Thread {
 public:
  ApplicationThread(
      const base::WeakPtr<CoreServicesApplicationDelegate>
          core_services_application,
      const std::string& name,
      scoped_ptr<mojo::ApplicationDelegate> delegate,
      mojo::InterfaceRequest<mojo::Application> request)
      : base::Thread(name),
        core_services_application_(core_services_application),
        core_services_application_task_runner_(
            base::MessageLoop::current()->task_runner()),
        delegate_(delegate.Pass()),
        request_(request.Pass()) {
  }

  ~ApplicationThread() override {
    Stop();
  }

  // Overridden from base::Thread:
  void Run(base::MessageLoop* message_loop) override {
    {
      application_impl_.reset(new mojo::ApplicationImpl(
          delegate_.get(), request_.Pass()));
      base::Thread::Run(message_loop);
      application_impl_.reset();
    }
    delegate_.reset();

    core_services_application_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CoreServicesApplicationDelegate::ApplicationThreadDestroyed,
                   core_services_application_,
                   this));

    // TODO(erg): This is a hack.
    //
    // Right now, most of our services do not receive
    // Application::RequestQuit() calls. jam@ is currently working on shutting
    // down everything cleanly. In the long run, we don't wan this here (we
    // want this set in ShutdownCleanly()), but until we can rely on
    // RequestQuit() getting delivered we have to manually do this here.
    //
    // Remove this ASAP.
    Thread::SetThreadWasQuitProperly(true);
  }

  void RequestQuit() {
    if (!IsRunning())
      return;
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ApplicationThread::ShutdownCleanly,
                   base::Unretained(this)));

  }

  void ShutdownCleanly() {
    static_cast<mojo::Application*>(application_impl_.get())->RequestQuit();
    Thread::SetThreadWasQuitProperly(true);
  }

 private:
  base::WeakPtr<CoreServicesApplicationDelegate> core_services_application_;
  scoped_refptr<base::SingleThreadTaskRunner>
      core_services_application_task_runner_;
  scoped_ptr<mojo::ApplicationImpl> application_impl_;
  scoped_ptr<mojo::ApplicationDelegate> delegate_;
  mojo::InterfaceRequest<mojo::Application> request_;

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

bool CoreServicesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void CoreServicesApplicationDelegate::Quit() {
  // Fire off RequestQuit() messages to all the threads before we try to join
  // on them.
  for (ApplicationThread* thread : application_threads_)
    thread->RequestQuit();

  // This will delete all threads. This also performs a blocking join, waiting
  // for the threads to end.
  application_threads_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void CoreServicesApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ContentHandler> request) {
  handler_bindings_.AddBinding(this, request.Pass());
}

void CoreServicesApplicationDelegate::StartApplication(
    mojo::InterfaceRequest<mojo::Application> request,
    mojo::URLResponsePtr response) {
  std::string url = response->url;

  scoped_ptr<mojo::ApplicationDelegate> delegate;
  if (url == "mojo://clipboard/")
    delegate.reset(new clipboard::ClipboardApplicationDelegate);
#if !defined(OS_ANDROID)
  else if (url == "mojo://native_viewport_service/")
    delegate.reset(new native_viewport::NativeViewportApplicationDelegate);
#endif
  else if (url == "mojo://network_service/")
    delegate.reset(new NetworkServiceDelegate);
#if !defined(OS_ANDROID)
  else if (url == "mojo://omnibox/")
    delegate.reset(new mandoline::OmniboxImpl);
#endif
  else if (url == "mojo://resource_provider/")
    delegate.reset(new resource_provider::ResourceProviderApp);
  else if (url == "mojo://surfaces_service/")
    delegate.reset(new surfaces::SurfacesServiceApplication);
  else if (url == "mojo://tracing/")
    delegate.reset(new tracing::TracingApp);
  else if (url == "mojo://view_manager/")
    delegate.reset(new view_manager::ViewManagerApp);
  else if (url == "mojo://window_manager/")
    delegate.reset(new mandoline::Browser);
  else
    NOTREACHED() << "This application package does not support " << url;

  base::Thread::Options thread_options;

  // In the case of mojo:network_service, we must use an IO message loop.
  if (url == "mojo://network_service/") {
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  } else if (url == "mojo://native_viewport_service/") {
    thread_options.message_loop_type = base::MessageLoop::TYPE_UI;
  } else {
    // We must use a MessagePumpMojo to awake on mojo messages.
    thread_options.message_pump_factory =
        base::Bind(&mojo::common::MessagePumpMojo::Create);
  }

  scoped_ptr<ApplicationThread> thread(
      new ApplicationThread(weak_factory_.GetWeakPtr(), url, delegate.Pass(),
      request.Pass()));
  thread->StartWithOptions(thread_options);

  application_threads_.push_back(thread.Pass());
}

}  // namespace core_services
