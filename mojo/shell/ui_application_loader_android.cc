// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/ui_application_loader_android.h"

#include "base/bind.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/shell/context.h"

namespace mojo {

class UIApplicationLoader::UILoader {
 public:
  explicit UILoader(ApplicationLoader* loader) : loader_(loader) {}
  ~UILoader() {}

  void Load(ApplicationManager* manager,
            const GURL& url,
            ScopedMessagePipeHandle shell_handle) {
    scoped_refptr<LoadCallbacks> callbacks(
        new ApplicationLoader::SimpleLoadCallbacks(shell_handle.Pass()));
    loader_->Load(manager, url, callbacks);
  }

  void OnServiceError(ApplicationManager* manager, const GURL& url) {
    loader_->OnServiceError(manager, url);
  }

 private:
  ApplicationLoader* loader_;  // Owned by UIApplicationLoader

  DISALLOW_COPY_AND_ASSIGN(UILoader);
};

UIApplicationLoader::UIApplicationLoader(
    scoped_ptr<ApplicationLoader> real_loader,
    shell::Context* context)
    : loader_(real_loader.Pass()), context_(context) {
}

UIApplicationLoader::~UIApplicationLoader() {
  context_->ui_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIApplicationLoader::ShutdownOnUIThread,
                 base::Unretained(this)));
}

void UIApplicationLoader::Load(ApplicationManager* manager,
                               const GURL& url,
                               scoped_refptr<LoadCallbacks> callbacks) {
  ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
  if (!shell_handle.is_valid())
    return;
  context_->ui_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &UIApplicationLoader::LoadOnUIThread,
          base::Unretained(this),
          manager,
          url,
          base::Owned(new ScopedMessagePipeHandle(shell_handle.Pass()))));
}

void UIApplicationLoader::OnServiceError(ApplicationManager* manager,
                                         const GURL& url) {
  context_->ui_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIApplicationLoader::OnServiceErrorOnUIThread,
                 base::Unretained(this),
                 manager,
                 url));
}

void UIApplicationLoader::LoadOnUIThread(
    ApplicationManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle* shell_handle) {
  if (!ui_loader_)
    ui_loader_ = new UILoader(loader_.get());
  ui_loader_->Load(manager, url, shell_handle->Pass());
}

void UIApplicationLoader::OnServiceErrorOnUIThread(ApplicationManager* manager,
                                                   const GURL& url) {
  if (!ui_loader_)
    ui_loader_ = new UILoader(loader_.get());
  ui_loader_->OnServiceError(manager, url);
}

void UIApplicationLoader::ShutdownOnUIThread() {
  delete ui_loader_;
  // Destroy |loader_| on the thread it's actually used on.
  loader_.reset();
}

}  // namespace mojo
