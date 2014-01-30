// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/dynamic_service_loader.h"

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/scoped_native_library.h"
#include "base/threading/simple_thread.h"
#include "mojo/shell/context.h"
#include "mojo/shell/switches.h"

typedef MojoResult (*MojoMainFunction)(MojoHandle pipe);

namespace mojo {
namespace shell {

class DynamicServiceLoader::LoadContext
    : public mojo::shell::Loader::Delegate,
      public base::DelegateSimpleThread::Delegate {
 public:
  LoadContext(DynamicServiceLoader* loader,
              const GURL& url,
              ScopedMessagePipeHandle service_handle)
      : loader_(loader),
        url_(url),
        service_handle_(service_handle.Pass()),
        weak_factory_(this) {
    url_ = url;
    if (url.scheme() == "mojo") {
      const CommandLine& command_line = *CommandLine::ForCurrentProcess();
      std::string origin =
          command_line.GetSwitchValueASCII(switches::kOrigin);
#if defined(OS_WIN)
      std::string lib(url.ExtractFileName() + ".dll");
#elif defined(OS_LINUX)
      std::string lib("lib" + url.ExtractFileName() + ".so");
#elif defined(OS_MACOSX)
      std::string lib("lib" + url.ExtractFileName() + ".dylib");
#else
      std::string lib;
      NOTREACHED() << "dynamic loading of services not supported";
      return;
#endif
      url_ = GURL(origin + std::string("/") + lib);
    }
    request_ = loader_->context_->loader()->Load(url_, this);
  }

 private:
  friend class base::WeakPtrFactory<LoadContext>;

  // From Loader::Delegate.
  virtual void DidCompleteLoad(const GURL& app_url,
                               const base::FilePath& app_path) OVERRIDE {
    app_path_ = app_path;
    ack_closure_ =
        base::Bind(&DynamicServiceLoader::LoadContext::AppCompleted,
                   weak_factory_.GetWeakPtr());
    thread_.reset(new base::DelegateSimpleThread(this, "app_thread"));
    thread_->Start();
  }

  // From base::DelegateSimpleThread::Delegate.
  virtual void Run() OVERRIDE {
    base::ScopedClosureRunner app_deleter(
        base::Bind(base::IgnoreResult(&base::DeleteFile), app_path_, false));
    std::string load_error;
    base::ScopedNativeLibrary app_library(
        base::LoadNativeLibrary(app_path_, &load_error));
    if (!app_library.is_valid()) {
      LOG(ERROR) << "Failed to load library: "
                 << app_path_.value()
                 << " (" << url_.spec() << ")";
      LOG(ERROR) << "error: " << load_error;
      return;
    }

    MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
        app_library.GetFunctionPointer("MojoMain"));
    if (!main_function) {
      LOG(ERROR) << "Entrypoint MojoMain not found.";
      return;
    }

    MojoHandle handle = service_handle_.release().value();
    // |MojoMain()| takes ownership of the app handle.
    MojoResult result = main_function(handle);
    if (result < MOJO_RESULT_OK) {
      LOG(ERROR) << "MojoMain returned an error: " << result;
      return;
    }
    loader_->context_->task_runners()->ui_runner()->PostTask(
        FROM_HERE,
        ack_closure_);
  }

  void AppCompleted() {
    thread_->Join();
    thread_.reset();
    loader_->url_to_load_context_.erase(url_);
    delete this;
  }

  DynamicServiceLoader* loader_;
  GURL url_;
  base::FilePath app_path_;
  base::Closure ack_closure_;
  scoped_ptr<mojo::shell::Loader::Job> request_;
  scoped_ptr<base::DelegateSimpleThread> thread_;
  ScopedMessagePipeHandle service_handle_;
  base::WeakPtrFactory<LoadContext> weak_factory_;
};

DynamicServiceLoader::DynamicServiceLoader(Context* context)
    : context_(context) {
}
DynamicServiceLoader::~DynamicServiceLoader() {}

void DynamicServiceLoader::Load(const GURL& url,
                                ScopedMessagePipeHandle service_handle) {
  DCHECK(url_to_load_context_.find(url) == url_to_load_context_.end());
  url_to_load_context_[url] = new LoadContext(this, url, service_handle.Pass());
}

}  // namespace shell
}  // namespace mojo
