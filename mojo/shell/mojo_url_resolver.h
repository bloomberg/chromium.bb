// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_MOJO_URL_RESOLVER_H_
#define MOJO_SHELL_MOJO_URL_RESOLVER_H_

#include <map>
#include <set>

#include "url/gurl.h"

namespace mojo {
namespace shell {

// This class resolves "mojo:" URLs to physical URLs (e.g., "file:" and
// "https:" URLs). By default, "mojo:" URLs resolve to a file location, but
// that resolution can be customized via the AddCustomMapping method.
class MojoURLResolver {
 public:
  MojoURLResolver();
  ~MojoURLResolver();

  // If specified, then unknown "mojo:" URLs will be resolved relative to this
  // base URL. That is, the portion after the colon will be appeneded to
  // |base_url| with platform-specific shared library prefix and suffix
  // inserted.
  void SetBaseURL(const GURL& base_url);

  // Add a custom mapping for a particular "mojo:" URL.
  void AddCustomMapping(const GURL& mojo_url, const GURL& resolved_url);

  // Add a local file mapping for a particular "mojo:" URL. This causes the
  // "mojo:" URL to be resolved to a base::DIR_MODULE-relative shared library.
  void AddLocalFileMapping(const GURL& mojo_url);

  // Resolve the given "mojo:" URL to the URL that should be used to fetch the
  // code for the corresponding Mojo App.
  GURL Resolve(const GURL& mojo_url) const;

 private:
  std::map<GURL, GURL> url_map_;
  std::set<GURL> local_file_set_;
  GURL default_base_url_;
  GURL base_url_;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_MOJO_URL_RESOLVER_H_
