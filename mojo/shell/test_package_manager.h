// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TEST_PACKAGE_MANAGER_H_
#define MOJO_SHELL_TEST_PACKAGE_MANAGER_H_

#include <map>
#include <string>

#include "mojo/shell/package_manager.h"

class GURL;

namespace mojo {
namespace shell {

// An implementation of PackageManager used by tests to support content handler
// registration for MIME types.
class TestPackageManager : public PackageManager {
 public:
  TestPackageManager();
  ~TestPackageManager() override;

  void RegisterContentHandler(const std::string& mime_type,
                              const GURL& content_handler_url);

 private:
  using MimeTypeToURLMap = std::map<std::string, GURL>;

  // Overridden from PackageManager:
  void SetApplicationManager(ApplicationManager* manager) override;
  GURL ResolveURL(const GURL& url) override;
  void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) override;
  bool HandleWithContentHandler(Fetcher* fetcher,
                                const GURL& unresolved_url,
                                base::TaskRunner* task_runner,
                                URLResponsePtr* new_response,
                                GURL* content_handler_url,
                                std::string* qualifier) override;

  MimeTypeToURLMap mime_type_to_url_;

  DISALLOW_COPY_AND_ASSIGN(TestPackageManager);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TEST_PACKAGE_MANAGER_H_
