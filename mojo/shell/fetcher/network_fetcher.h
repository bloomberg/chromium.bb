// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FETCHER_NETWORK_FETCHER_H_
#define MOJO_SHELL_FETCHER_NETWORK_FETCHER_H_

#include "mojo/shell/fetcher.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "url/gurl.h"

namespace mojo {

class URLLoaderFactory;

namespace shell {

// Implements Fetcher for http[s] files.
class NetworkFetcher : public Fetcher {
 public:
  NetworkFetcher(bool disable_cache,
                 mojo::URLRequestPtr request,
                 URLLoaderFactory* url_loader_factory,
                 const FetchCallback& loader_callback);

  ~NetworkFetcher() override;

 private:
  // TODO(hansmuller): Revisit this when a real peek operation is available.
  static const MojoDeadline kPeekTimeout = MOJO_DEADLINE_INDEFINITE;

  const GURL& GetURL() const override;
  GURL GetRedirectURL() const override;
  GURL GetRedirectReferer() const override;

  URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                               uint32_t skip) override;

  static void RecordCacheToURLMapping(const base::FilePath& path,
                                      const GURL& url);

  // AppIds should be be both predictable and unique, but any hash would work.
  // Currently we use sha256 from crypto/secure_hash.h
  static bool ComputeAppId(const base::FilePath& path,
                           std::string* digest_string);

  static bool RenameToAppId(const GURL& url,
                            const base::FilePath& old_path,
                            base::FilePath* new_path);

  void CopyCompleted(base::Callback<void(const base::FilePath&, bool)> callback,
                     bool success);

  void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) override;

  std::string MimeType() override;

  bool HasMojoMagic() override;

  bool PeekFirstLine(std::string* line) override;

  void StartNetworkRequest(mojo::URLRequestPtr request,
                           URLLoaderFactory* url_loader_factory);

  void OnLoadComplete(URLResponsePtr response);

  bool disable_cache_;
  const GURL url_;
  URLLoaderPtr url_loader_;
  URLResponsePtr response_;
  base::FilePath path_;
  base::WeakPtrFactory<NetworkFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcher);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_FETCHER_NETWORK_FETCHER_H_
