// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_FETCHER_UPDATE_FETCHER_H_
#define MOJO_FETCHER_UPDATE_FETCHER_H_

#include "mojo/shell/fetcher.h"

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/updater/updater.mojom.h"
#include "url/gurl.h"

namespace mojo {

class URLLoaderFactory;

namespace fetcher {

class UpdateFetcher : public shell::Fetcher {
 public:
  UpdateFetcher(const GURL& url,
                updater::Updater* updater,
                const FetchCallback& loader_callback);

  ~UpdateFetcher() override;

 private:
  // Fetcher implementation:
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

  void OnGetAppPath(const mojo::String& path);

  const GURL url_;
  base::FilePath path_;
  base::WeakPtrFactory<UpdateFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateFetcher);
};

}  // namespace fetcher
}  // namespace mojo

#endif  // MOJO_FETCHER_UPDATE_FETCHER_H_
