// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/android_loader.h"

#include "components/mus/mus_app.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace mus {

AndroidLoader::AndroidLoader() {}
AndroidLoader::~AndroidLoader() {}

void AndroidLoader::Load(
    const GURL& url,
    mojo::InterfaceRequest<mojo::shell::mojom::ShellClient> request) {
  DCHECK(request.is_pending());
  app_.reset(new mojo::ShellConnection(new MandolineUIServicesApp,
                                       std::move(request)));
}

}  // namespace mus
