// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FETCHER_DATA_FETCHER_H_
#define MOJO_SHELL_FETCHER_DATA_FETCHER_H_

#include "mojo/shell/fetcher.h"

#include <stdint.h>

#include "base/macros.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

// Implements Fetcher for data: URLs.
class DataFetcher : public Fetcher {
 public:
  static void Start(const GURL& url, const FetchCallback& loader_callback);

 private:
  DataFetcher(const GURL& url, const FetchCallback& loader_callback);
  ~DataFetcher() override;

  void BuildAndDispatchResponse();

  // Fetcher implementation.
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

  const GURL url_;
  URLResponsePtr response_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcher);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_FETCHER_DATA_FETCHER_H_
