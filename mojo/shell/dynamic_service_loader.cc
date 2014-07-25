// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/dynamic_service_loader.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "mojo/shell/context.h"
#include "mojo/shell/keep_alive.h"
#include "mojo/shell/switches.h"
#include "net/base/filename_util.h"

namespace mojo {
namespace shell {
namespace {

class Loader {
 public:
  explicit Loader(scoped_ptr<DynamicServiceRunner> runner)
      : runner_(runner.Pass()) {
  }

  virtual void Start(const GURL& url,
                     ScopedMessagePipeHandle service_handle,
                     Context* context) = 0;

  void StartService(const base::FilePath& path,
                    ScopedMessagePipeHandle service_handle,
                    bool path_is_valid) {
    if (path_is_valid) {
      runner_->Start(path, service_handle.Pass(),
                     base::Bind(&Loader::AppCompleted, base::Unretained(this)));
    } else {
      AppCompleted();
    }
  }

 protected:
  virtual ~Loader() {}

 private:
  void AppCompleted() {
    delete this;
  }

  scoped_ptr<DynamicServiceRunner> runner_;
};

// For loading services via file:// URLs.
class LocalLoader : public Loader {
 public:
  explicit LocalLoader(scoped_ptr<DynamicServiceRunner> runner)
      : Loader(runner.Pass()) {
  }

  virtual void Start(const GURL& url,
                     ScopedMessagePipeHandle service_handle,
                     Context* context) OVERRIDE {
    base::FilePath path;
    net::FileURLToFilePath(url, &path);

    // TODO(darin): Check if the given file path exists.

    // Complete asynchronously for consistency with NetworkServiceLoader.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&Loader::StartService,
                   base::Unretained(this),
                   path,
                   base::Passed(&service_handle),
                   true));
  }
};

// For loading services via the network stack.
class NetworkLoader : public Loader {
 public:
  explicit NetworkLoader(scoped_ptr<DynamicServiceRunner> runner,
                         NetworkService* network_service)
      : Loader(runner.Pass()) {
    network_service->CreateURLLoader(Get(&url_loader_));
  }

  virtual void Start(const GURL& url,
                    ScopedMessagePipeHandle service_handle,
                    Context* context) OVERRIDE {
    service_handle_ = service_handle.Pass();
    context_ = context;

    URLRequestPtr request(URLRequest::New());
    request->url = String::From(url);
    request->auto_follow_redirects = true;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableCache)) {
      request->bypass_cache = true;
    }

    url_loader_->Start(request.Pass(),
                       base::Bind(&NetworkLoader::OnReceivedResponse,
                                  base::Unretained(this)));
  }

 private:
  virtual ~NetworkLoader() {
    if (!file_.empty())
      base::DeleteFile(file_, false);
  }

  void OnReceivedResponse(URLResponsePtr response) {
    if (response->error) {
      LOG(ERROR) << "Error (" << response->error->code << ": "
                 << response->error->description << ") while fetching "
                 << response->url;
    }

    base::CreateTemporaryFile(&file_);
    common::CopyToFile(response->body.Pass(),
                       file_,
                       context_->task_runners()->blocking_pool(),
                       base::Bind(&Loader::StartService,
                                  base::Unretained(this),
                                  file_,
                                  base::Passed(&service_handle_)));
  }

  Context* context_;
  NetworkServicePtr network_service_;
  URLLoaderPtr url_loader_;
  ScopedMessagePipeHandle service_handle_;
  base::FilePath file_;
};

}  // namespace

DynamicServiceLoader::DynamicServiceLoader(
    Context* context,
    scoped_ptr<DynamicServiceRunnerFactory> runner_factory)
    : context_(context),
      runner_factory_(runner_factory.Pass()) {
}

DynamicServiceLoader::~DynamicServiceLoader() {
}

void DynamicServiceLoader::LoadService(ServiceManager* manager,
                                       const GURL& url,
                                       ScopedMessagePipeHandle shell_handle) {
  scoped_ptr<DynamicServiceRunner> runner = runner_factory_->Create(context_);

  GURL resolved_url;
  if (url.SchemeIs("mojo")) {
    resolved_url = context_->mojo_url_resolver()->Resolve(url);
  } else {
    resolved_url = url;
  }

  Loader* loader;
  if (resolved_url.SchemeIsFile()) {
    loader = new LocalLoader(runner.Pass());
  } else {
    if (!network_service_) {
      context_->service_manager()->ConnectToService(
          GURL("mojo:mojo_network_service"),
          &network_service_);
    }
    loader = new NetworkLoader(runner.Pass(), network_service_.get());
  }
  loader->Start(resolved_url, shell_handle.Pass(), context_);
}

void DynamicServiceLoader::OnServiceError(ServiceManager* manager,
                                          const GURL& url) {
  // TODO(darin): What should we do about service errors? This implies that
  // the app closed its handle to the service manager. Maybe we don't care?
}

}  // namespace shell
}  // namespace mojo
