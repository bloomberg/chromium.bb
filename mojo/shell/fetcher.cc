// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher.h"

#include <stddef.h>

#include "url/gurl.h"

namespace mojo {
namespace shell {

const char Fetcher::kMojoMagic[] = "#!mojo ";
const size_t Fetcher::kMaxShebangLength = 2048;

Fetcher::Fetcher(const FetchCallback& loader_callback)
    : loader_callback_(loader_callback) {
}

Fetcher::~Fetcher() {
}

bool Fetcher::PeekContentHandler(std::string* mojo_shebang,
                                 GURL* mojo_content_handler_url) {
  // TODO(aa): I guess this should just go in ApplicationManager now.
  std::string shebang;
  if (HasMojoMagic() && PeekFirstLine(&shebang)) {
    GURL url(shebang.substr(arraysize(kMojoMagic) - 1, std::string::npos));
    if (url.is_valid()) {
      *mojo_shebang = shebang;
      *mojo_content_handler_url = url;
      return true;
    }
  }
  return false;
}

}  // namespace shell
}  // namespace mojo
