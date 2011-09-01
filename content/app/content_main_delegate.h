// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_MAIN_DELEGATE_H_
#define CONTENT_APP_CONTENT_MAIN_DELEGATE_H_
#pragma once

namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}

namespace content {

class ContentMainDelegate {
 public:
  // Tells the embedder that the absolute basic startup has been done, i.e. it's
  // now safe to create singeltons and check the command line. Return true if
  // the process should exit afterwards, and if so, |exit_code| should be set.
  virtual bool BasicStartupComplete(
      int* exit_code,
      base::mac::ScopedNSAutoreleasePool* autorelease_pool) = 0;
};

}  // namespace content

#endif  // CONTENT_APP_CONTENT_MAIN_DELEGATE_H_
