// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/dynamic_application_loader.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "mojo/shell/context.h"
#include "mojo/shell/switches.h"
#include "net/base/filename_util.h"

namespace mojo {
namespace shell {

// Encapsulates loading and running one individual application.
//
// Loaders are owned by DynamicApplicationLoader. DynamicApplicationLoader must
// ensure that all the parameters passed to Loader subclasses stay valid through
// Loader's lifetime.
//
// Async operations are done with WeakPtr to protect against
// DynamicApplicationLoader going away (and taking all the Loaders with it)
// while the async operation is outstanding.
class DynamicApplicationLoader::Loader {
 public:
  Loader(Context* context,
         DynamicServiceRunnerFactory* runner_factory,
         scoped_refptr<ApplicationLoader::LoadCallbacks> load_callbacks,
         const LoaderCompleteCallback& loader_complete_callback)
      : load_callbacks_(load_callbacks),
        loader_complete_callback_(loader_complete_callback),
        context_(context),
        runner_factory_(runner_factory),
        weak_ptr_factory_(this) {}

  virtual ~Loader() {}

 protected:
  void RunLibrary(const base::FilePath& path, bool path_exists) {
    ScopedMessagePipeHandle shell_handle =
        load_callbacks_->RegisterApplication();
    if (!shell_handle.is_valid()) {
      LoaderComplete();
      return;
    }

    if (!path_exists) {
      DVLOG(1) << "Library not started because library path '" << path.value()
               << "' does not exist.";
      LoaderComplete();
      return;
    }

    runner_ = runner_factory_->Create(context_);
    runner_->Start(
        path,
        shell_handle.Pass(),
        base::Bind(&Loader::LoaderComplete, weak_ptr_factory_.GetWeakPtr()));
  }

  void LoaderComplete() { loader_complete_callback_.Run(this); }

  scoped_refptr<ApplicationLoader::LoadCallbacks> load_callbacks_;
  LoaderCompleteCallback loader_complete_callback_;
  Context* context_;

 private:
  DynamicServiceRunnerFactory* runner_factory_;
  scoped_ptr<DynamicServiceRunner> runner_;
  base::WeakPtrFactory<Loader> weak_ptr_factory_;
};

// A loader for local files.
class DynamicApplicationLoader::LocalLoader : public Loader {
 public:
  LocalLoader(const GURL& url,
              Context* context,
              DynamicServiceRunnerFactory* runner_factory,
              scoped_refptr<ApplicationLoader::LoadCallbacks> load_callbacks,
              const LoaderCompleteCallback& loader_complete_callback)
      : Loader(context,
               runner_factory,
               load_callbacks,
               loader_complete_callback),
        weak_ptr_factory_(this) {
    base::FilePath path;
    net::FileURLToFilePath(url, &path);

    // Async for consistency with network case.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LocalLoader::RunLibrary,
                   weak_ptr_factory_.GetWeakPtr(),
                   path,
                   base::PathExists(path)));
  }

  virtual ~LocalLoader() {}

 private:
  base::WeakPtrFactory<LocalLoader> weak_ptr_factory_;
};

// A loader for network files.
class DynamicApplicationLoader::NetworkLoader : public Loader {
 public:
  NetworkLoader(const GURL& url,
                MimeTypeToURLMap* mime_type_to_url,
                Context* context,
                DynamicServiceRunnerFactory* runner_factory,
                NetworkService* network_service,
                scoped_refptr<ApplicationLoader::LoadCallbacks> load_callbacks,
                const LoaderCompleteCallback& loader_complete_callback)
      : Loader(context,
               runner_factory,
               load_callbacks,
               loader_complete_callback),
        mime_type_to_url_(mime_type_to_url),
        weak_ptr_factory_(this) {
    URLRequestPtr request(URLRequest::New());
    request->url = String::From(url);
    request->auto_follow_redirects = true;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableCache)) {
      request->bypass_cache = true;
    }

    network_service->CreateURLLoader(Get(&url_loader_));
    url_loader_->Start(request.Pass(),
                       base::Bind(&NetworkLoader::OnLoadComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~NetworkLoader() {
    if (!file_.empty())
      base::DeleteFile(file_, false);
  }

 private:
  void OnLoadComplete(URLResponsePtr response) {
    if (response->error) {
      LOG(ERROR) << "Error (" << response->error->code << ": "
                 << response->error->description << ") while fetching "
                 << response->url;
      LoaderComplete();
      return;
    }

    MimeTypeToURLMap::iterator iter =
        mime_type_to_url_->find(response->mime_type);
    if (iter != mime_type_to_url_->end()) {
      load_callbacks_->LoadWithContentHandler(iter->second, response.Pass());
      return;
    }

    base::CreateTemporaryFile(&file_);
    common::CopyToFile(
        response->body.Pass(),
        file_,
        context_->task_runners()->blocking_pool(),
        base::Bind(
            &NetworkLoader::RunLibrary, weak_ptr_factory_.GetWeakPtr(), file_));
  }

  MimeTypeToURLMap* mime_type_to_url_;
  URLLoaderPtr url_loader_;
  base::FilePath file_;
  base::WeakPtrFactory<NetworkLoader> weak_ptr_factory_;
};

DynamicApplicationLoader::DynamicApplicationLoader(
    Context* context,
    scoped_ptr<DynamicServiceRunnerFactory> runner_factory)
    : context_(context),
      runner_factory_(runner_factory.Pass()),

      // Unretained() is correct here because DynamicApplicationLoader owns the
      // loaders that we pass this callback to.
      loader_complete_callback_(
          base::Bind(&DynamicApplicationLoader::LoaderComplete,
                     base::Unretained(this))) {
}

DynamicApplicationLoader::~DynamicApplicationLoader() {
}

void DynamicApplicationLoader::RegisterContentHandler(
    const std::string& mime_type,
    const GURL& content_handler_url) {
  mime_type_to_url_[mime_type] = content_handler_url;
}

void DynamicApplicationLoader::Load(
    ApplicationManager* manager,
    const GURL& url,
    scoped_refptr<LoadCallbacks> load_callbacks) {
  GURL resolved_url;
  if (url.SchemeIs("mojo")) {
    resolved_url = context_->mojo_url_resolver()->Resolve(url);
  } else {
    resolved_url = url;
  }

  if (resolved_url.SchemeIsFile()) {
    loaders_.push_back(new LocalLoader(resolved_url,
                                       context_,
                                       runner_factory_.get(),
                                       load_callbacks,
                                       loader_complete_callback_));
    return;
  }

  if (!network_service_) {
    context_->application_manager()->ConnectToService(
        GURL("mojo:mojo_network_service"), &network_service_);
  }

  loaders_.push_back(new NetworkLoader(resolved_url,
                                       &mime_type_to_url_,
                                       context_,
                                       runner_factory_.get(),
                                       network_service_.get(),
                                       load_callbacks,
                                       loader_complete_callback_));
}

void DynamicApplicationLoader::OnApplicationError(ApplicationManager* manager,
                                                  const GURL& url) {
  // TODO(darin): What should we do about service errors? This implies that
  // the app closed its handle to the service manager. Maybe we don't care?
}

void DynamicApplicationLoader::LoaderComplete(Loader* loader) {
  loaders_.erase(std::find(loaders_.begin(), loaders_.end(), loader));
}

}  // namespace shell
}  // namespace mojo
