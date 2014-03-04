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
#include "mojo/shell/keep_alive.h"
#include "mojo/shell/switches.h"

typedef MojoResult (*MojoMainFunction)(MojoHandle pipe);

namespace mojo {
namespace shell {
namespace {

std::string MakeSharedLibraryName(const std::string& file_name) {
#if defined(OS_WIN)
  return file_name + ".dll";
#elif defined(OS_LINUX)
  return "lib" + file_name + ".so";
#elif defined(OS_MACOSX)
  return "lib" + file_name + ".dylib";
#else
  NOTREACHED() << "dynamic loading of services not supported";
  return std::string();
#endif
}

}  // namespace

class DynamicServiceLoader::LoadContext
    : public mojo::shell::Loader::Delegate,
      public base::DelegateSimpleThread::Delegate {
 public:
  LoadContext(DynamicServiceLoader* loader,
              const GURL& url,
              ScopedShellHandle service_handle)
      : thread_(this, "app_thread"),
        loader_(loader),
        url_(url),
        service_handle_(service_handle.Pass()),
        keep_alive_(loader->context_) {
    GURL url_to_load;

    if (url.SchemeIs("mojo")) {
      std::string origin =
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kOrigin);
      std::string lib = MakeSharedLibraryName(url.ExtractFileName());
      url_to_load = GURL(origin + "/" + lib);
    } else {
      url_to_load = url;
    }

    request_ = loader_->context_->loader()->Load(url_to_load, this);
  }

  virtual ~LoadContext() {
    thread_.Join();
  }

 private:
  // From Loader::Delegate.
  virtual void DidCompleteLoad(const GURL& app_url,
                               const base::FilePath& app_path,
                               const std::string* mime_type) OVERRIDE {
    app_path_ = app_path;
    thread_.Start();
  }

  // From base::DelegateSimpleThread::Delegate.
  virtual void Run() OVERRIDE {
    base::ScopedClosureRunner app_deleter(
        base::Bind(base::IgnoreResult(&base::DeleteFile), app_path_, false));

    do {
      std::string load_error;
      base::ScopedNativeLibrary app_library(
          base::LoadNativeLibrary(app_path_, &load_error));
      if (!app_library.is_valid()) {
        LOG(ERROR) << "Failed to load library: " << app_path_.value() << " ("
                   << url_.spec() << ")";
        LOG(ERROR) << "error: " << load_error;
        break;
      }

      MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
          app_library.GetFunctionPointer("MojoMain"));
      if (!main_function) {
        LOG(ERROR) << "Entrypoint MojoMain not found.";
        break;
      }

      // |MojoMain()| takes ownership of the service handle.
      // TODO(darin): What if MojoMain does not close the service handle?
      MojoResult result = main_function(service_handle_.release().value());
      if (result < MOJO_RESULT_OK)
        LOG(ERROR) << "MojoMain returned an error: " << result;
    } while (false);

    loader_->context_->task_runners()->ui_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LoadContext::AppCompleted, base::Unretained(this)));
  }

  void AppCompleted() {
    loader_->AppCompleted(url_);
  }

  base::DelegateSimpleThread thread_;
  DynamicServiceLoader* loader_;
  GURL url_;
  base::FilePath app_path_;
  scoped_ptr<mojo::shell::Loader::Job> request_;
  ScopedShellHandle service_handle_;
  KeepAlive keep_alive_;
};

DynamicServiceLoader::DynamicServiceLoader(Context* context)
    : context_(context) {
}

DynamicServiceLoader::~DynamicServiceLoader() {
  DCHECK(url_to_load_context_.empty());
}

void DynamicServiceLoader::LoadService(ServiceManager* manager,
                                       const GURL& url,
                                       ScopedShellHandle service_handle) {
  DCHECK(url_to_load_context_.find(url) == url_to_load_context_.end());
  url_to_load_context_[url] = new LoadContext(this, url, service_handle.Pass());
}

void DynamicServiceLoader::AppCompleted(const GURL& url) {
  LoadContextMap::iterator it = url_to_load_context_.find(url);
  DCHECK(it != url_to_load_context_.end());

  LoadContext* doomed = it->second;
  url_to_load_context_.erase(it);

  delete doomed;
}

}  // namespace shell
}  // namespace mojo
