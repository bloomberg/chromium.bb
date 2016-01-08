// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FETCHER_ABOUT_FETCHER_H_
#define MOJO_SHELL_FETCHER_ABOUT_FETCHER_H_

#include "mojo/shell/fetcher.h"

#include <stdint.h>

#include "base/macros.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

// Implements Fetcher for about: URLs.
class AboutFetcher : public shell::Fetcher {
 public:
  static const char kAboutScheme[];
  static const char kAboutBlankURL[];

  static void Start(const GURL& url, const FetchCallback& loader_callback);

 private:
  AboutFetcher(const GURL& url, const FetchCallback& loader_callback);
  ~AboutFetcher() override;

  void BuildResponse();

  // Must be called exactly once to run the loader callback (asynchrously). On
  // success, the ownership of this object is passed to the loader callback;
  // otherwise, the callback is run with a nullptr and this object is destroyed.
  void PostToRunCallback(bool success);

  // shell::Fetcher implementation.
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

  DISALLOW_COPY_AND_ASSIGN(AboutFetcher);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_FETCHER_ABOUT_FETCHER_H_
