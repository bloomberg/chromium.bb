// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_

#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"

namespace subresource_filter {
namespace testing {

proto::UrlRule CreateSuffixRule(base::StringPiece suffix);

proto::UrlRule CreateWhitelistRuleForDocument(base::StringPiece pattern);

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_
