// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/tools/package_manager/package_manager_application.h"

#include "base/files/file_util.h"
#include "mojo/tools/package_manager/manifest.h"
#include "mojo/tools/package_manager/unpacker.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/application/application_impl.h"

namespace mojo {

namespace {

const base::FilePath::CharType kManifestFileName[] =
    FILE_PATH_LITERAL("manifest.json");

}  // namespace

PackageManagerApplication::PendingLoad::PendingLoad() {
}

PackageManagerApplication::PendingLoad::~PendingLoad() {
}

PackageManagerApplication::PackageManagerApplication() {
}

PackageManagerApplication::~PackageManagerApplication() {
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

  StartLoad(url);
}

void PackageManagerApplication::StartLoad(const GURL& url) {
  if (completed_.find(url) != completed_.end() ||
      pending_.find(url) != pending_.end())
    return;  // Already loaded or are loading this one.

  PendingLoad* load = new PendingLoad;
  load->fetcher.reset(new mojo::PackageFetcher(
      network_service_.get(), this, url));
  pending_[url] = load;
}

void PackageManagerApplication::LoadDeps(const Manifest& manifest) {
  for (size_t i = 0; i < manifest.deps().size(); i++)
    StartLoad(manifest.deps()[i]);
}

void PackageManagerApplication::PendingLoadComplete(const GURL& url) {
  pending_.erase(pending_.find(url));
  completed_.insert(url);
  if (pending_.empty())
    base::MessageLoop::current()->Quit();
}

void PackageManagerApplication::FetchSucceeded(
    const GURL& url,
    const base::FilePath& name) {
  Unpacker unpacker;
  if (!unpacker.Unpack(name)) {
    base::DeleteFile(name, false);
    printf("Failed to unpack\n");
    PendingLoadComplete(url);
    return;
  }
  // The downloaded .zip file is no longer necessary.
  base::DeleteFile(name, false);

  // Load the manifest.
  base::FilePath manifest_path = unpacker.dir().Append(kManifestFileName);
  Manifest manifest;
  std::string err_msg;
  if (!manifest.ParseFromFile(manifest_path, &err_msg)) {
    printf("%s\n", err_msg.c_str());
    PendingLoadComplete(url);
    return;
  }

  // Enqueue loads for any deps.
  LoadDeps(manifest);

  printf("Loaded %s\n", url.spec().c_str());
  PendingLoadComplete(url);
}

void PackageManagerApplication::FetchFailed(const GURL& url) {
  PendingLoadComplete(url);
}

}  // namespace mojo
