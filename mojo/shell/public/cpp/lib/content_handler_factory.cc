// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/content_handler_factory.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"

namespace mojo {

namespace {

class ApplicationThread : public base::PlatformThread::Delegate {
 public:
  ApplicationThread(
      scoped_refptr<base::SingleThreadTaskRunner> handler_thread,
      const base::Callback<void(ApplicationThread*)>& termination_callback,
      ContentHandlerFactory::Delegate* handler_delegate,
      InterfaceRequest<shell::mojom::Application> application_request,
      URLResponsePtr response,
      const Callback<void()>& destruct_callback)
      : handler_thread_(handler_thread),
        termination_callback_(termination_callback),
        handler_delegate_(handler_delegate),
        application_request_(std::move(application_request)),
        response_(std::move(response)),
        destruct_callback_(destruct_callback) {}

  ~ApplicationThread() override {
    destruct_callback_.Run();
  }

 private:
  void ThreadMain() override {
    handler_delegate_->RunApplication(std::move(application_request_),
                                      std::move(response_));
    handler_thread_->PostTask(FROM_HERE,
                              base::Bind(termination_callback_, this));
  }

  scoped_refptr<base::SingleThreadTaskRunner> handler_thread_;
  base::Callback<void(ApplicationThread*)> termination_callback_;
  ContentHandlerFactory::Delegate* handler_delegate_;
  InterfaceRequest<shell::mojom::Application> application_request_;
  URLResponsePtr response_;
  Callback<void()> destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationThread);
};

class ContentHandlerImpl : public shell::mojom::ContentHandler {
 public:
  ContentHandlerImpl(ContentHandlerFactory::Delegate* delegate,
                     InterfaceRequest<shell::mojom::ContentHandler> request)
      : delegate_(delegate),
        binding_(this, std::move(request)),
        weak_factory_(this) {}
  ~ContentHandlerImpl() override {
    // We're shutting down and doing cleanup. Cleanup may trigger calls back to
    // OnThreadEnd(). As we're doing the cleanup here we don't want to do it in
    // OnThreadEnd() as well. InvalidateWeakPtrs() ensures we don't get any
    // calls to OnThreadEnd().
    weak_factory_.InvalidateWeakPtrs();
    for (auto thread : active_threads_) {
      base::PlatformThread::Join(thread.second);
      delete thread.first;
    }
  }

 private:
  // Overridden from ContentHandler:
  void StartApplication(
      InterfaceRequest<shell::mojom::Application> application_request,
      URLResponsePtr response,
      const Callback<void()>& destruct_callback) override {
    ApplicationThread* thread =
        new ApplicationThread(base::ThreadTaskRunnerHandle::Get(),
                              base::Bind(&ContentHandlerImpl::OnThreadEnd,
                                         weak_factory_.GetWeakPtr()),
                              delegate_, std::move(application_request),
                              std::move(response), destruct_callback);
    base::PlatformThreadHandle handle;
    bool launched = base::PlatformThread::Create(0, thread, &handle);
    DCHECK(launched);
    active_threads_[thread] = handle;
  }

  void OnThreadEnd(ApplicationThread* thread) {
    DCHECK(active_threads_.find(thread) != active_threads_.end());
    base::PlatformThreadHandle handle = active_threads_[thread];
    active_threads_.erase(thread);
    base::PlatformThread::Join(handle);
    delete thread;
  }

  ContentHandlerFactory::Delegate* delegate_;
  std::map<ApplicationThread*, base::PlatformThreadHandle> active_threads_;
  StrongBinding<shell::mojom::ContentHandler> binding_;
  base::WeakPtrFactory<ContentHandlerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

}  // namespace

ContentHandlerFactory::ContentHandlerFactory(Delegate* delegate)
    : delegate_(delegate) {
}

ContentHandlerFactory::~ContentHandlerFactory() {
}

void ContentHandlerFactory::ManagedDelegate::RunApplication(
    InterfaceRequest<shell::mojom::Application> application_request,
    URLResponsePtr response) {
  base::MessageLoop loop(common::MessagePumpMojo::Create());
  auto application = this->CreateApplication(std::move(application_request),
                                             std::move(response));
  if (application)
    loop.Run();
}

void ContentHandlerFactory::Create(
    Connection* connection,
    InterfaceRequest<shell::mojom::ContentHandler> request) {
  new ContentHandlerImpl(delegate_, std::move(request));
}

}  // namespace mojo
