// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blacklist.h"

#include <set>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/blacklist.h"

namespace extensions {

TestBlacklist::TestBlacklist(Blacklist* blacklist)
    : blacklist_(blacklist) {
}

namespace {

void Assign(Blacklist::BlacklistState *out, Blacklist::BlacklistState in) {
  *out = in;
}

}  // namespace

Blacklist::BlacklistState TestBlacklist::GetBlacklistState(
    const std::string& extension_id) {
  Blacklist::BlacklistState blacklist_state;
  blacklist_->IsBlacklisted(extension_id,
                            base::Bind(&Assign, &blacklist_state));
  base::RunLoop().RunUntilIdle();
  return blacklist_state;
}

}  // namespace extensions
