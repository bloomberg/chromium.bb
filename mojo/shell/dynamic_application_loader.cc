// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/dynamic_application_loader.h"

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
#include "mojo/shell/switches.h"
#include "net/base/filename_util.h"

namespace mojo {
namespace shell {

DynamicApplicationLoader::DynamicApplicationLoader(
    Context* context,
    scoped_ptr<DynamicServiceRunnerFactory> runner_factory)
    : context_(context),
      runner_factory_(runner_factory.Pass()),
      weak_ptr_factory_(this) {
}

DynamicApplicationLoader::~DynamicApplicationLoader() {
}

void DynamicApplicationLoader::RegisterContentHandler(
    const std::string& mime_type,
    const GURL& content_handler_url) {
  mime_type_to_url_[mime_type] = content_handler_url;
}

void DynamicApplicationLoader::Load(ApplicationManager* manager,
                                    const GURL& url,
                                    scoped_refptr<LoadCallbacks> callbacks) {
  GURL resolved_url;
  if (url.SchemeIs("mojo")) {
    resolved_url = context_->mojo_url_resolver()->Resolve(url);
  } else {
    resolved_url = url;
  }

  if (resolved_url.SchemeIsFile())
    LoadLocalService(resolved_url, callbacks);
  else
    LoadNetworkService(resolved_url, callbacks);
}

void DynamicApplicationLoader::LoadLocalService(
    const GURL& resolved_url,
    scoped_refptr<LoadCallbacks> callbacks) {
  base::FilePath path;
  net::FileURLToFilePath(resolved_url, &path);
  const bool kDeleteFileAfter = false;

  // Async for consistency with network case.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DynamicApplicationLoader::RunLibrary,
                 weak_ptr_factory_.GetWeakPtr(),
                 path,
                 callbacks,
                 kDeleteFileAfter,
                 base::PathExists(path)));
}

void DynamicApplicationLoader::LoadNetworkService(
    const GURL& resolved_url,
    scoped_refptr<LoadCallbacks> callbacks) {
  if (!network_service_) {
    context_->application_manager()->ConnectToService(
        GURL("mojo:mojo_network_service"), &network_service_);
  }
  if (!url_loader_)
    network_service_->CreateURLLoader(Get(&url_loader_));

  URLRequestPtr request(URLRequest::New());
  request->url = String::From(resolved_url);
  request->auto_follow_redirects = true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCache)) {
    request->bypass_cache = true;
  }

  url_loader_->Start(
      request.Pass(),
      base::Bind(&DynamicApplicationLoader::OnLoadNetworkServiceComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 callbacks));
}

void DynamicApplicationLoader::OnLoadNetworkServiceComplete(
    scoped_refptr<LoadCallbacks> callbacks,
    URLResponsePtr response) {
  if (response->error) {
    LOG(ERROR) << "Error (" << response->error->code << ": "
               << response->error->description << ") while fetching "
               << response->url;
  }

  MimeTypeToURLMap::iterator iter = mime_type_to_url_.find(response->mime_type);
  if (iter != mime_type_to_url_.end()) {
    callbacks->LoadWithContentHandler(iter->second, response.Pass());
    return;
  }

  base::FilePath file;
  base::CreateTemporaryFile(&file);

  const bool kDeleteFileAfter = true;
  common::CopyToFile(response->body.Pass(),
                     file,
                     context_->task_runners()->blocking_pool(),
                     base::Bind(&DynamicApplicationLoader::RunLibrary,
                                weak_ptr_factory_.GetWeakPtr(),
                                file,
                                callbacks,
                                kDeleteFileAfter));
}

void DynamicApplicationLoader::RunLibrary(
    const base::FilePath& path,
    scoped_refptr<LoadCallbacks> callbacks,
    bool delete_file_after,
    bool path_exists) {

  ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
  if (!shell_handle.is_valid())
    return;

  if (!path_exists) {
    DVLOG(1) << "Library not started because library path '"
             << path.value() << "' does not exist.";
    return;
  }

  scoped_ptr<DynamicServiceRunner> runner =
      runner_factory_->Create(context_).Pass();
  runner->Start(path,
                shell_handle.Pass(),
                base::Bind(&DynamicApplicationLoader::OnRunLibraryComplete,
                           weak_ptr_factory_.GetWeakPtr(),
                           base::Unretained(runner.get()),
                           delete_file_after ? path : base::FilePath()));
  runners_.push_back(runner.release());
}

void DynamicApplicationLoader::OnRunLibraryComplete(
    DynamicServiceRunner* runner,
    const base::FilePath& temp_file) {
  for (ScopedVector<DynamicServiceRunner>::iterator it = runners_.begin();
       it != runners_.end(); ++it) {
    if (*it == runner) {
      runners_.erase(it);
      if (!temp_file.empty())
        base::DeleteFile(temp_file, false);
      return;
    }
  }
}

void DynamicApplicationLoader::OnApplicationError(ApplicationManager* manager,
                                                  const GURL& url) {
  // TODO(darin): What should we do about service errors? This implies that
  // the app closed its handle to the service manager. Maybe we don't care?
}

}  // namespace shell
}  // namespace mojo
