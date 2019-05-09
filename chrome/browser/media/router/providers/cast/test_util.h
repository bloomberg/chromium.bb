// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_TEST_UTIL_H_

#include <iosfwd>

#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

std::ostream& operator<<(std::ostream&, CastInternalMessage::Type);
std::ostream& operator<<(std::ostream&, const CastInternalMessage&);

// Matcher for CastInternalMessage arguments.
MATCHER_P(IsCastInternalMessage, json, "") {
  auto message = CastInternalMessage::From(base::test::ParseJson(json));
  DCHECK(message);
  if (arg.type() != message->type() ||
      arg.client_id() != message->client_id() ||
      arg.sequence_number() != message->sequence_number()) {
    return false;
  }

  if (arg.has_session_id() && arg.session_id() != message->session_id())
    return false;

  switch (arg.type()) {
    case CastInternalMessage::Type::kAppMessage:
      return arg.app_message_namespace() == message->app_message_namespace() &&
             arg.app_message_body() == message->app_message_body();
    case CastInternalMessage::Type::kV2Message:
      return arg.v2_message_type() == message->v2_message_type() &&
             testing::Matches(base::test::IsJson(arg.v2_message_body()))(
                 message->v2_message_body());
    default:
      return true;
  }
}

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_TEST_UTIL_H_
