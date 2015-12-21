// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TEST_PACKAGE_MANAGER_H_
#define MOJO_SHELL_TEST_PACKAGE_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "mojo/shell/package_manager.h"

namespace mojo {
namespace shell {
namespace test {

class TestPackageManager : public PackageManager {
 public:
  TestPackageManager();
  ~TestPackageManager() override;

 private:
  // Overridden from PackageManager:
  void SetApplicationManager(ApplicationManager* manager) override;
  void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) override;
  uint32_t HandleWithContentHandler(
      Fetcher* fetcher,
      const Identity& source,
      const GURL& target_url,
      const CapabilityFilter& target_filter,
      InterfaceRequest<Application>* application_request) override;

  DISALLOW_COPY_AND_ASSIGN(TestPackageManager);
};

}  // namespace test
}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TEST_PACKAGE_MANAGER_H_
