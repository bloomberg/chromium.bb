// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/updater/updater_impl.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "mandoline/services/updater/updater_app.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "url/gurl.h"

namespace updater {

UpdaterImpl::UpdaterImpl(mojo::ApplicationImpl* app_impl,
                         UpdaterApp* application,
                         mojo::InterfaceRequest<Updater> request)
    : application_(application),
      app_impl_(app_impl),
      binding_(this, std::move(request)) {}

UpdaterImpl::~UpdaterImpl() {
}

void UpdaterImpl::GetPathForApp(
    const mojo::String& url,
    const Updater::GetPathForAppCallback& callback) {
  // TODO(scottmg): For now, just stub to local files. This will become an
  // integration with components/update_client.
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  const GURL as_url(url);
  const base::FilePath path = shell_dir.Append(base::FilePath::FromUTF8Unsafe(
      as_url.path().substr(2, as_url.path().size() - 3) + ".mojo"));
  callback.Run(path.AsUTF8Unsafe());
}

}  // namespace updater
