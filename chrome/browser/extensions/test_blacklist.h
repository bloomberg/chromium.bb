// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_BLACKLIST_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_BLACKLIST_H_

#include <string>

#include "base/basictypes.h"

namespace extensions {

class Blacklist;

// A wrapper for an extensions::Blacklist that provides functionality for
// testing.
class TestBlacklist {
 public:
  explicit TestBlacklist(Blacklist* blacklist);

  Blacklist* blacklist() { return blacklist_; }

  bool IsBlacklisted(const std::string& extension_id);

 private:
  Blacklist* blacklist_;

  DISALLOW_COPY_AND_ASSIGN(TestBlacklist);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_BLACKLIST_H_
