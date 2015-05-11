// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/core_services_application_delegate.h"

#include "base/bind.h"
#include "components/clipboard/clipboard_application_delegate.h"
#include "components/view_manager/view_manager_app.h"
#include "mandoline/ui/browser/browser.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/services/tracing/tracing_app.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "mojo/services/network/network_service_delegate.h"
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
  ApplicationThread(const std::string& name,
                    scoped_ptr<mojo::ApplicationDelegate> delegate,
                    mojo::InterfaceRequest<mojo::Application> request)
      : base::Thread(name),
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
  scoped_ptr<mojo::ApplicationImpl> application_impl_;
  scoped_ptr<mojo::ApplicationDelegate> delegate_;
  mojo::InterfaceRequest<mojo::Application> request_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationThread);
};

CoreServicesApplicationDelegate::CoreServicesApplicationDelegate() {}

CoreServicesApplicationDelegate::~CoreServicesApplicationDelegate() {
  application_threads_.clear();
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
  else if (url == "mojo://network_service/")
    delegate.reset(new NetworkServiceDelegate);
#endif
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
  } else {
    // We must use a MessagePumpMojo to awake on mojo messages.
    thread_options.message_pump_factory =
        base::Bind(&mojo::common::MessagePumpMojo::Create);
  }

  scoped_ptr<ApplicationThread> thread(
      new ApplicationThread(url, delegate.Pass(), request.Pass()));
  thread->StartWithOptions(thread_options);

  application_threads_.push_back(thread.Pass());
}

}  // namespace core_services
