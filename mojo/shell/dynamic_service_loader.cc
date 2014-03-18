// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/dynamic_service_loader.h"

#include "base/command_line.h"
#include "base/location.h"
#include "mojo/shell/context.h"
#include "mojo/shell/keep_alive.h"
#include "mojo/shell/switches.h"

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

class DynamicServiceLoader::LoadContext : public mojo::shell::Loader::Delegate {
 public:
  LoadContext(DynamicServiceLoader* loader,
              const GURL& url,
              ScopedShellHandle service_handle,
              scoped_ptr<DynamicServiceRunner> runner)
      : loader_(loader),
        url_(url),
        service_handle_(service_handle.Pass()),
        runner_(runner.Pass()),
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
  }

 private:
  // |Loader::Delegate| method:
  virtual void DidCompleteLoad(const GURL& app_url,
                               const base::FilePath& app_path,
                               const std::string* mime_type) OVERRIDE {
    DVLOG(2) << "Completed load of " << app_url << " (" << url_ << ") to "
             << app_path.value();

    runner_->Start(
        app_path,
        service_handle_.Pass(),
        base::Bind(&LoadContext::AppCompleted,
                   scoped_refptr<base::TaskRunner>(
                       loader_->context_->task_runners()->ui_runner()),
                   base::Unretained(loader_), url_));
  }

  static void AppCompleted(scoped_refptr<base::TaskRunner> task_runner,
                           DynamicServiceLoader* loader,
                           const GURL& url) {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(&DynamicServiceLoader::AppCompleted,
                                     base::Unretained(loader), url));
  }

  DynamicServiceLoader* const loader_;
  const GURL url_;
  scoped_ptr<mojo::shell::Loader::Job> request_;
  ScopedShellHandle service_handle_;
  scoped_ptr<DynamicServiceRunner> runner_;
  KeepAlive keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(LoadContext);
};

DynamicServiceLoader::DynamicServiceLoader(
    Context* context,
    scoped_ptr<DynamicServiceRunnerFactory> runner_factory)
    : context_(context),
      runner_factory_(runner_factory.Pass()) {
}

DynamicServiceLoader::~DynamicServiceLoader() {
  DCHECK(url_to_load_context_.empty());
}

void DynamicServiceLoader::LoadService(ServiceManager* manager,
                                       const GURL& url,
                                       ScopedShellHandle service_handle) {
  DCHECK(url_to_load_context_.find(url) == url_to_load_context_.end());
  url_to_load_context_[url] = new LoadContext(
      this, url, service_handle.Pass(), runner_factory_->Create(context_));
}

void DynamicServiceLoader::AppCompleted(const GURL& url) {
  LoadContextMap::iterator it = url_to_load_context_.find(url);
  DCHECK(it != url_to_load_context_.end()) << url;

  LoadContext* doomed = it->second;
  url_to_load_context_.erase(it);

  delete doomed;
}

}  // namespace shell
}  // namespace mojo
