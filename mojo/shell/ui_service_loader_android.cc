// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/ui_service_loader_android.h"

#include "base/bind.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/shell/context.h"

namespace mojo {

class UIServiceLoader::UILoader {
 public:
  explicit UILoader(ServiceLoader* loader) : loader_(loader) {}
  ~UILoader() {}

  void Load(ServiceManager* manager,
            const GURL& url,
            ScopedMessagePipeHandle shell_handle) {
    scoped_refptr<LoadCallbacks> callbacks(
        new ServiceLoader::SimpleLoadCallbacks(shell_handle.Pass()));
    loader_->Load(manager, url, callbacks);
  }

  void OnServiceError(ServiceManager* manager, const GURL& url) {
    loader_->OnServiceError(manager, url);
  }

 private:
  ServiceLoader* loader_;  // Owned by UIServiceLoader

  DISALLOW_COPY_AND_ASSIGN(UILoader);
};

UIServiceLoader::UIServiceLoader(scoped_ptr<ServiceLoader> real_loader,
                                 shell::Context* context)
    : loader_(real_loader.Pass()), context_(context) {
}

UIServiceLoader::~UIServiceLoader() {
  context_->ui_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIServiceLoader::ShutdownOnUIThread, base::Unretained(this)));
}

void UIServiceLoader::Load(ServiceManager* manager,
                           const GURL& url,
                           scoped_refptr<LoadCallbacks> callbacks) {
  ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
  if (!shell_handle.is_valid())
    return;
  context_->ui_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIServiceLoader::LoadOnUIThread,
                 base::Unretained(this),
                 manager,
                 url,
                 base::Owned(
                     new ScopedMessagePipeHandle(shell_handle.Pass()))));
}

void UIServiceLoader::OnServiceError(ServiceManager* manager, const GURL& url) {
  context_->ui_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIServiceLoader::OnServiceErrorOnUIThread,
                 base::Unretained(this),
                 manager,
                 url));
}

void UIServiceLoader::LoadOnUIThread(ServiceManager* manager,
                                     const GURL& url,
                                     ScopedMessagePipeHandle* shell_handle) {
  if (!ui_loader_)
    ui_loader_ = new UILoader(loader_.get());
  ui_loader_->Load(manager, url, shell_handle->Pass());
}

void UIServiceLoader::OnServiceErrorOnUIThread(ServiceManager* manager,
                                               const GURL& url) {
  if (!ui_loader_)
    ui_loader_ = new UILoader(loader_.get());
  ui_loader_->OnServiceError(manager, url);
}

void UIServiceLoader::ShutdownOnUIThread() {
  delete ui_loader_;
  // Destroy |loader_| on the thread it's actually used on.
  loader_.reset();
}

}  // namespace mojo
