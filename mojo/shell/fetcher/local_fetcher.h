// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FETCHER_LOCAL_FETCHER_H_
#define MOJO_SHELL_FETCHER_LOCAL_FETCHER_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "mojo/shell/fetcher.h"
#include "url/gurl.h"

namespace mojo {

class NetworkService;

namespace shell {

// Implements Fetcher for file:// URLs.
class LocalFetcher : public Fetcher {
 public:
  LocalFetcher(NetworkService* network_service,
               const GURL& url,
               const GURL& url_without_query,
               const base::FilePath& shell_file_root,
               const FetchCallback& loader_callback);

  ~LocalFetcher() override;

  void GetMimeTypeFromFileCallback(const mojo::String& mime_type);

 private:
  const GURL& GetURL() const override;
  GURL GetRedirectURL() const override;
  GURL GetRedirectReferer() const override;

  URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                               uint32_t skip) override;

  void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) override;

  std::string MimeType() override;

  bool HasMojoMagic() override;

  bool PeekFirstLine(std::string* line) override;

  GURL url_;
  base::FilePath path_;
  base::FilePath shell_file_root_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(LocalFetcher);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_FETCHER_LOCAL_FETCHER_H_
