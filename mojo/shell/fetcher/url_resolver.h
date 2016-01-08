// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FETCHER_URL_RESOLVER_H_
#define MOJO_SHELL_FETCHER_URL_RESOLVER_H_

#include <map>
#include <set>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

// Supports resolving "mojo:" URLs to a file location, with ".mojo" appended.
class URLResolver {
 public:
  explicit URLResolver(const GURL& mojo_base_url);
  ~URLResolver();

  // Resolve the given "mojo:" URL to the URL that should be used to fetch the
  // code for the corresponding Mojo App.
  GURL ResolveMojoURL(const GURL& mojo_url) const;

 private:
  GURL mojo_base_url_;

  DISALLOW_COPY_AND_ASSIGN(URLResolver);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_FETCHER_URL_RESOLVER_H_
