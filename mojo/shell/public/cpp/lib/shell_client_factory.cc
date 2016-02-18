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
#include "mojo/common/url_type_converters.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"
#include "mojo/shell/public/cpp/shell_client_factory.h"
#include "url/gurl.h"

namespace mojo {

namespace {

class ApplicationThread : public base::PlatformThread::Delegate {
 public:
  ApplicationThread(
      scoped_refptr<base::SingleThreadTaskRunner> handler_thread,
      const base::Callback<void(ApplicationThread*)>& termination_callback,
      ShellClientFactory::Delegate* handler_delegate,
      InterfaceRequest<shell::mojom::ShellClient> request,
      const GURL& url)
      : handler_thread_(handler_thread),
        termination_callback_(termination_callback),
        handler_delegate_(handler_delegate),
        request_(std::move(request)),
        url_(url) {}

  ~ApplicationThread() override {}

 private:
  void ThreadMain() override {
    handler_delegate_->CreateShellClient(std::move(request_), url_);
    handler_thread_->PostTask(FROM_HERE,
                              base::Bind(termination_callback_, this));
  }

  scoped_refptr<base::SingleThreadTaskRunner> handler_thread_;
  base::Callback<void(ApplicationThread*)> termination_callback_;
  ShellClientFactory::Delegate* handler_delegate_;
  InterfaceRequest<shell::mojom::ShellClient> request_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationThread);
};

class ShellClientFactoryImpl : public shell::mojom::ShellClientFactory {
 public:
  ShellClientFactoryImpl(mojo::ShellClientFactory::Delegate* delegate,
                         shell::mojom::ShellClientFactoryRequest request)
      : delegate_(delegate),
        binding_(this, std::move(request)),
        weak_factory_(this) {}
  ~ShellClientFactoryImpl() override {
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
  // Overridden from shell::mojom::ShellClientFactory:
  void CreateShellClient(shell::mojom::ShellClientRequest request,
                         const String& url) override {
    ApplicationThread* thread =
        new ApplicationThread(base::ThreadTaskRunnerHandle::Get(),
                              base::Bind(&ShellClientFactoryImpl::OnThreadEnd,
                                         weak_factory_.GetWeakPtr()),
                              delegate_, std::move(request), url.To<GURL>());
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

  mojo::ShellClientFactory::Delegate* delegate_;
  std::map<ApplicationThread*, base::PlatformThreadHandle> active_threads_;
  StrongBinding<shell::mojom::ShellClientFactory> binding_;
  base::WeakPtrFactory<ShellClientFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellClientFactoryImpl);
};

}  // namespace

ShellClientFactory::ShellClientFactory(Delegate* delegate)
    : delegate_(delegate) {
}

ShellClientFactory::~ShellClientFactory() {
}

void ShellClientFactory::ManagedDelegate::CreateShellClient(
    shell::mojom::ShellClientRequest request,
    const GURL& url) {
  base::MessageLoop loop(common::MessagePumpMojo::Create());
  auto application = this->CreateShellClientManaged(std::move(request), url);
  if (application)
    loop.Run();
}

void ShellClientFactory::Create(
    Connection* connection,
    shell::mojom::ShellClientFactoryRequest request) {
  new ShellClientFactoryImpl(delegate_, std::move(request));
}

}  // namespace mojo
