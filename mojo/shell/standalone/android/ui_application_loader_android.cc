// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/android/ui_application_loader_android.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/application_manager.h"

namespace mojo {
namespace shell {

UIApplicationLoader::UIApplicationLoader(
    scoped_ptr<ApplicationLoader> real_loader,
    base::MessageLoop* ui_message_loop)
    : loader_(std::move(real_loader)), ui_message_loop_(ui_message_loop) {}

UIApplicationLoader::~UIApplicationLoader() {
  ui_message_loop_->PostTask(
      FROM_HERE, base::Bind(&UIApplicationLoader::ShutdownOnUIThread,
                            base::Unretained(this)));
}

void UIApplicationLoader::Load(
    const GURL& url,
    InterfaceRequest<mojom::Application> application_request) {
  DCHECK(application_request.is_pending());
  ui_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&UIApplicationLoader::LoadOnUIThread, base::Unretained(this),
                 url, base::Passed(&application_request)));
}

void UIApplicationLoader::LoadOnUIThread(
    const GURL& url,
    InterfaceRequest<mojom::Application> application_request) {
  loader_->Load(url, std::move(application_request));
}

void UIApplicationLoader::ShutdownOnUIThread() {
  // Destroy |loader_| on the thread it's actually used on.
  loader_.reset();
}

}  // namespace shell
}  // namespace mojo
