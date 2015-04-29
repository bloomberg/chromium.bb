// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_URL_RESOLVER_H_
#define MOJO_RUNNER_URL_RESOLVER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "url/gurl.h"

namespace mojo {
namespace runner {

// This class supports the mapping of URLs to other URLs.
// It's commonly used with mojo: URL, to provide a physical location (i.e.
// file: or https:) but works with any URL.
// By default, "mojo:" URLs resolve to a file location, with ".mojo" appended,
// but that resolution can be customized via the AddCustomMapping method.
class URLResolver {
 public:
  URLResolver();
  ~URLResolver();

  // Used for the return value of GetOriginMappings().
  struct OriginMapping {
    OriginMapping(const std::string& origin, const std::string& base_url)
        : origin(origin), base_url(base_url) {}

    std::string origin;
    std::string base_url;
  };

  // Returns a list of origin mappings based on command line args.
  // The switch --map-origin can be specified multiple times. Each occurance
  // has the format of --map-origin={origin}={base_url}
  // For example:
  //   --map-origin=http://domokit.org=file:///source/out
  static std::vector<OriginMapping> GetOriginMappings(
      const base::CommandLine::StringVector& argv);

  // Add a custom mapping for a particular URL. If |mapped_url| is
  // itself a mojo url normal resolution rules apply.
  void AddURLMapping(const GURL& url, const GURL& mapped_url);

  // Add a custom mapping for all urls rooted at |origin|.
  void AddOriginMapping(const GURL& origin, const GURL& base_url);

  // Applies all custom mappings for |url|, returning the last non-mapped url.
  // For example, if 'a' maps to 'b' and 'b' maps to 'c' calling this with 'a'
  // returns 'c'.
  GURL ApplyMappings(const GURL& url) const;

  // If specified, then "mojo:" URLs will be resolved relative to this
  // URL. That is, the portion after the colon will be appeneded to
  // |mojo_base_url| with .mojo appended.
  void SetMojoBaseURL(const GURL& mojo_base_url);

  // Resolve the given "mojo:" URL to the URL that should be used to fetch the
  // code for the corresponding Mojo App.
  GURL ResolveMojoURL(const GURL& mojo_url) const;

 private:
  using GURLToGURLMap = std::map<GURL, GURL>;
  GURLToGURLMap url_map_;
  GURLToGURLMap origin_map_;
  GURL mojo_base_url_;

  DISALLOW_COPY_AND_ASSIGN(URLResolver);
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_URL_RESOLVER_H_
