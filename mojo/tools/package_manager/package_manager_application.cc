// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/tools/package_manager/package_manager_application.h"

#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/application/application_impl.h"

namespace mojo {

PackageManagerApplication::PendingLoad::PendingLoad() {
}

PackageManagerApplication::PendingLoad::~PendingLoad() {
}

PackageManagerApplication::PackageManagerApplication() {
}

PackageManagerApplication::~PackageManagerApplication() {
  printf("APPLICATION EXITING\n");
  STLDeleteContainerPairSecondPointers(pending_.begin(), pending_.end());
}

void PackageManagerApplication::Initialize(ApplicationImpl* app) {
  app->ConnectToService("mojo:mojo_network_service", &network_service_);

  printf("Enter URL> ");
  char buf[1024];
  if (scanf("%1023s", buf) != 1) {
    printf("No input, exiting\n");
    base::MessageLoop::current()->Quit();
    return;
  }
  GURL url(buf);
  if (!url.is_valid()) {
    printf("Invalid URL\n");
    base::MessageLoop::current()->Quit();
    return;
  }

  PendingLoad* load = new PendingLoad;
  load->fetcher.reset(new mojo::PackageFetcher(
      network_service_.get(), this, url));

  pending_[url] = load;
}

void PackageManagerApplication::PendingLoadComplete(const GURL& url) {
  pending_.erase(pending_.find(url));
  if (pending_.empty())
    base::MessageLoop::current()->Quit();
}

void PackageManagerApplication::FetchSucceeded(
    const GURL& url,
    const base::FilePath& name) {
  PendingLoad* load = pending_.find(url)->second;

  if (!load->unpacker.Unpack(name)) {
    base::DeleteFile(name, false);
    printf("Failed to unpack\n");
    PendingLoadComplete(url);
    return;
  }
  base::DeleteFile(name, false);

  PendingLoadComplete(url);
}

void PackageManagerApplication::FetchFailed(const GURL& url) {
  PendingLoadComplete(url);
}

}  // namespace mojo
