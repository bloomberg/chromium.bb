// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/android_loader.h"

#include "components/mus/view_manager_app.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace view_manager {

AndroidLoader::AndroidLoader() {}
AndroidLoader::~AndroidLoader() {}

void AndroidLoader::Load(
    const GURL& url,
    mojo::InterfaceRequest<mojo::Application> application_request) {
  DCHECK(application_request.is_pending());
  app_.reset(new mojo::ApplicationImpl(new ViewManagerApp,
                                       application_request.Pass()));
}

}  // namespace view_manager
