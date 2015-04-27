// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/query_util.h"

#include "base/strings/string_util.h"

namespace mojo {
namespace shell {

GURL GetBaseURLAndQuery(const GURL& url, std::string* query) {
  if (!url.has_query()) {
    if (query)
      *query = "";
    return url;
  }

  if (query)
    *query = "?" + url.query();
  GURL::Replacements repl;
  repl.SetQueryStr("");
  std::string result = url.ReplaceComponents(repl).spec();

  // Remove the dangling '?' because it's ugly.
  base::ReplaceChars(result, "?", "", &result);
  return GURL(result);
}

}  // namespace shell
}  // namespace mojo
