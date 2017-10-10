// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/test_utils.h"

#include <string>

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions {
namespace declarative_net_request {

bool HasValidIndexedRuleset(const Extension& extension) {
  base::AssertBlockingAllowed();

  std::string data;
  if (!base::ReadFileToString(
          file_util::GetIndexedRulesetPath(extension.path()), &data)) {
    return false;
  }

  flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(data.c_str()),
                                 data.size());
  return flat::VerifyExtensionIndexedRulesetBuffer(verifier);
}

}  // namespace declarative_net_request
}  // namespace extensions
