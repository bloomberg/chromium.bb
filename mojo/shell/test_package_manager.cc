// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "mojo/shell/test_package_manager.h"

#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace test {

TestPackageManager::TestPackageManager() {}
TestPackageManager::~TestPackageManager() {}

void TestPackageManager::SetApplicationManager(ApplicationManager* manager) {}
void TestPackageManager::BuiltinAppLoaded(const GURL& url) {}
void TestPackageManager::FetchRequest(
    URLRequestPtr request,
    const Fetcher::FetchCallback& loader_callback) {}
uint32_t TestPackageManager::HandleWithContentHandler(
    Fetcher* fetcher,
    const Identity& source,
    const GURL& target_url,
    const CapabilityFilter& target_filter,
    InterfaceRequest<shell::mojom::ShellClient>* request) {
  return 0;
}
bool TestPackageManager::IsURLInCatalog(const std::string& url) const {
  return true;
}
std::string TestPackageManager::GetApplicationName(
    const std::string& url) const { return url; }
GURL TestPackageManager::ResolveMojoURL(const GURL& mojo_url) {
  return mojo_url;
}
uint32_t TestPackageManager::StartContentHandler(
    const Identity& source,
    const Identity& content_handler,
    const GURL& url,
    mojom::ShellClientRequest request) {
  return mojo::shell::mojom::Shell::kInvalidApplicationID;
}

}  // namespace test
}  // namespace shell
}  // namespace mojo
