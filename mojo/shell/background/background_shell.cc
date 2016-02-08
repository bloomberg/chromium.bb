// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/background/background_shell.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/standalone/context.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace {

scoped_ptr<base::MessagePump> CreateMessagePumpMojo() {
  return make_scoped_ptr(new common::MessagePumpMojo);
}

// Used to obtain the InterfaceRequest for an application.
class BackgroundApplicationLoader : public ApplicationLoader {
 public:
  BackgroundApplicationLoader() {}
  ~BackgroundApplicationLoader() override {}

  bool got_request() const { return got_request_; }
  InterfaceRequest<mojom::Application> TakeApplicationRequest() {
    return std::move(application_request_);
  }

  // ApplicationLoader:
  void Load(const GURL& url,
            InterfaceRequest<mojom::Application> application_request) override {
    got_request_ = true;
    application_request_ = std::move(application_request);
  }

 private:
  bool got_request_ = false;
  InterfaceRequest<mojom::Application> application_request_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundApplicationLoader);
};

class MojoMessageLoop : public base::MessageLoop {
 public:
  MojoMessageLoop()
      : base::MessageLoop(base::MessageLoop::TYPE_CUSTOM,
                          base::Bind(&CreateMessagePumpMojo)) {}
  ~MojoMessageLoop() override {}

  void BindToCurrentThread() { base::MessageLoop::BindToCurrentThread(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoMessageLoop);
};

}  // namespace

// Manages the thread to startup mojo.
class BackgroundShell::MojoThread : public base::SimpleThread {
 public:
  MojoThread() : SimpleThread("mojo-background-shell") {}
  ~MojoThread() override {}

  void CreateApplicationImpl(base::WaitableEvent* signal,
                             scoped_ptr<ConnectToApplicationParams> params,
                             InterfaceRequest<mojom::Application>* request) {
    // Only valid to call this on the background thread.
    DCHECK_EQ(message_loop_, base::MessageLoop::current());

    // Ownership of |loader| passes to ApplicationManager.
    BackgroundApplicationLoader* loader = new BackgroundApplicationLoader;
    const GURL url = params->target().url();
    context_->application_manager()->SetLoaderForURL(make_scoped_ptr(loader),
                                                     url);
    context_->application_manager()->ConnectToApplication(std::move(params));
    DCHECK(loader->got_request());
    *request = loader->TakeApplicationRequest();
    // Trigger destruction of the loader.
    context_->application_manager()->SetLoaderForURL(nullptr, url);
    signal->Signal();
  }

  base::MessageLoop* message_loop() { return message_loop_; }

  // Stops the background thread.
  void Stop() {
    DCHECK_NE(message_loop_, base::MessageLoop::current());
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    Join();
  }

  // base::SimpleThread:
  void Start() override {
    DCHECK(!message_loop_);
    message_loop_ = new MojoMessageLoop;
    base::SimpleThread::Start();
  }
  void Run() override {
    // The construction/destruction order is very finicky and has to be done
    // in the order here.
    scoped_ptr<base::MessageLoop> message_loop(message_loop_);

    Context::EnsureEmbedderIsInitialized();

    message_loop_->BindToCurrentThread();

    base::FilePath shell_dir;
    PathService::Get(base::DIR_MODULE, &shell_dir);

    scoped_ptr<Context> context(new Context);
    context_ = context.get();
    context_->Init(shell_dir);

    message_loop_->Run();

    // Has to happen after run, but while messageloop still valid.
    context_->Shutdown();

    // Context has to be destroyed after the MessageLoop has been destroyed.
    message_loop.reset();
    context_ = nullptr;
  }

 private:
  // We own this. It's created on the main thread, but destroyed on the
  // background thread.
  MojoMessageLoop* message_loop_ = nullptr;
  // Created in Run() on the background thread.
  Context* context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MojoThread);
};

BackgroundShell::BackgroundShell() {}

BackgroundShell::~BackgroundShell() {
  thread_->Stop();
}

void BackgroundShell::Init() {
  DCHECK(!thread_);
  thread_.reset(new MojoThread);
  thread_->Start();
}

InterfaceRequest<mojom::Application> BackgroundShell::CreateApplication(
    const GURL& url) {
  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->SetTarget(
      Identity(url, std::string(), GetPermissiveCapabilityFilter()));
  InterfaceRequest<mojom::Application> request;
  base::WaitableEvent signal(true, false);
  thread_->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoThread::CreateApplicationImpl,
                            base::Unretained(thread_.get()), &signal,
                            base::Passed(&params), &request));
  signal.Wait();
  return request;
}

void RegisterLocalAliases(PackageManagerImpl* manager) {}

}  // namespace shell
}  // namespace mojo
