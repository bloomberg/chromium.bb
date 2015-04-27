// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_IDENTITY_H_
#define MOJO_SHELL_IDENTITY_H_

#include "url/gurl.h"

namespace mojo {
namespace shell {

/**
 * Represents the identity of an application. |url| is the url of the
 * application. |qualifier| is a string that allows to tie a specific instance
 * of an application to another. It is used by content handlers that need to be
 * run in the context of another application.
 */
struct Identity {
  Identity(const GURL& url, const std::string& qualifier);
  explicit Identity(const GURL& url);
  bool operator<(const Identity& other) const;

  const GURL url;
  const std::string qualifier;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_IDENTITY_H_
