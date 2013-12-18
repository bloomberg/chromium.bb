// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/service_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/scoped_native_library.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/shell/context.h"
#include "mojo/shell/switches.h"
#include "mojom/shell.h"

typedef MojoResult (*MojoMainFunction)(MojoHandle pipe);

namespace mojo {
namespace shell {

class ServiceManager::Service : public ShellStub {
 public:
  Service(ServiceManager* manager, const GURL& url)
      : manager_(manager),
        url_(url) {
    ScopedMessagePipeHandle shell_handle, service_handle;
    CreateMessagePipe(&shell_handle, &service_handle);
    shell_client_.reset(shell_handle.Pass());
    shell_client_.SetPeer(this);
    manager_->GetLoaderForURL(url)->Load(url, manager_, service_handle.Pass());
  }

  virtual ~Service() {}

  void ConnectToClient(ScopedMessagePipeHandle handle) {
    if (handle.is_valid())
      shell_client_->AcceptConnection(handle.Pass());
  }

  virtual void Connect(const String& url,
                       ScopedMessagePipeHandle client_pipe) MOJO_OVERRIDE {
    manager_->Connect(GURL(url.To<std::string>()), client_pipe.Pass());
  }

 private:
  ServiceManager* manager_;
  GURL url_;
  RemotePtr<ShellClient> shell_client_;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

class ServiceManager::DynamicLoader : public ServiceManager::Loader {
 public:
  explicit DynamicLoader(ServiceManager* manager) : manager_(manager) {}
  virtual ~DynamicLoader() {}

 private:
  class Context : public mojo::shell::Loader::Delegate,
                  public base::DelegateSimpleThread::Delegate {
   public:
    Context(DynamicLoader* loader,
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
#else
        std::string lib;
        NOTREACHED() << "dynamic loading of services not supported";
        return;
#endif
        url_ = GURL(origin + std::string("/") + lib);
      }
      request_ = loader_->manager_->context_->loader()->Load(url_, this);
    }

   private:
    friend class base::WeakPtrFactory<Context>;

    // From Loader::Delegate.
    virtual void DidCompleteLoad(const GURL& app_url,
                                 const base::FilePath& app_path) OVERRIDE {
      app_path_ = app_path;
      ack_closure_ =
          base::Bind(&ServiceManager::DynamicLoader::Context::AppCompleted,
                     weak_factory_.GetWeakPtr());
      thread_.reset(new base::DelegateSimpleThread(this, "app_thread"));
      thread_->Start();
    }

    // From base::DelegateSimpleThread::Delegate.
    virtual void Run() OVERRIDE {
      base::ScopedClosureRunner app_deleter(
          base::Bind(base::IgnoreResult(&base::DeleteFile), app_path_, false));
      base::ScopedNativeLibrary app_library(
          base::LoadNativeLibrary(app_path_, NULL));
      if (!app_library.is_valid()) {
        LOG(ERROR) << "Failed to load library: " << app_path_.value().c_str();
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
      loader_->manager_->context_->task_runners()->ui_runner()->PostTask(
          FROM_HERE,
          ack_closure_);
    }

    void AppCompleted() {
      thread_->Join();
      thread_.reset();
      loader_->url_to_context_.erase(url_);
      delete this;
    }

    DynamicLoader* loader_;
    GURL url_;
    base::FilePath app_path_;
    base::Closure ack_closure_;
    scoped_ptr<mojo::shell::Loader::Job> request_;
    scoped_ptr<base::DelegateSimpleThread> thread_;
    ScopedMessagePipeHandle service_handle_;
    base::WeakPtrFactory<Context> weak_factory_;
  };

  virtual void Load(const GURL& url,
                    ServiceManager* manager,
                    ScopedMessagePipeHandle service_handle)
      MOJO_OVERRIDE {
    DCHECK(url_to_context_.find(url) == url_to_context_.end());
    url_to_context_[url] = new Context(this, url, service_handle.Pass());
  }

  typedef std::map<GURL, Context*> ContextMap;
  ContextMap url_to_context_;
  ServiceManager* manager_;
};

ServiceManager::Loader::Loader() {}
ServiceManager::Loader::~Loader() {}

ServiceManager::ServiceManager(Context* context)
    : context_(context),
      default_loader_(new DynamicLoader(this)) {
}

ServiceManager::~ServiceManager() {
}

void ServiceManager::SetLoaderForURL(Loader* loader, const GURL& gurl) {
  DCHECK(url_to_loader_.find(gurl) == url_to_loader_.end());
  url_to_loader_[gurl] = loader;
}

ServiceManager::Loader* ServiceManager::GetLoaderForURL(const GURL& gurl) {
  LoaderMap::const_iterator it = url_to_loader_.find(gurl);
  if (it != url_to_loader_.end())
    return it->second;
  return default_loader_.get();
}

void ServiceManager::Connect(const GURL& url,
                             ScopedMessagePipeHandle client_handle) {
  ServiceMap::const_iterator service_it = url_to_service_.find(url);
  Service* service;
  if (service_it != url_to_service_.end()) {
    service = service_it->second;
  } else {
    service = new Service(this, url);
    url_to_service_[url] = service;
  }
  service->ConnectToClient(client_handle.Pass());
}

}  // namespace shell
}  // namespace mojo
