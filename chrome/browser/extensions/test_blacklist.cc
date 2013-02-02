// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blacklist.h"

#include <set>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/blacklist.h"

namespace extensions {

TestBlacklist::TestBlacklist(Blacklist* blacklist)
    : blacklist_(blacklist) {
}

namespace {

void Assign(std::set<std::string>* out, const std::set<std::string>& in) {
  *out = in;
}

}  // namespace

bool TestBlacklist::IsBlacklisted(const std::string& extension_id) {
  std::set<std::string> id_set;
  id_set.insert(extension_id);
  std::set<std::string> blacklist_set;
  blacklist_->GetBlacklistedIDs(id_set,
                                base::Bind(&Assign, &blacklist_set));
  base::RunLoop().RunUntilIdle();
  return blacklist_set.count(extension_id) > 0;
}

}  // namespace extensions
