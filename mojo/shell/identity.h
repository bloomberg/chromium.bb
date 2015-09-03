// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_IDENTITY_H_
#define MOJO_SHELL_IDENTITY_H_

#include "url/gurl.h"

namespace mojo {
namespace shell {

// Represents the identity of an application.
// |url| is the URL of the application.
// |qualifier| is a string that allows to tie a specific instance of an
// application to another. A typical use case of qualifier is to control process
// grouping for a given application URL. For example, the core services are
// grouped into "Core"/"Files"/"Network"/etc. using qualifier; content handler's
// qualifier is derived from the origin of the content.
struct Identity {
  Identity();
  Identity(const GURL& in_url, const std::string& in_qualifier);
  explicit Identity(const GURL& in_url);

  bool operator<(const Identity& other) const;
  bool is_null() const { return url.is_empty(); }

  GURL url;
  std::string qualifier;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_IDENTITY_H_
