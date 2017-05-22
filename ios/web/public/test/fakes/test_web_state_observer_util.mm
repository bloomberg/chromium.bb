// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state_observer_util.h"

#import "ios/web/public/web_state/navigation_context.h"

namespace web {
TestDidStartNavigationInfo::TestDidStartNavigationInfo() {}
TestDidStartNavigationInfo::~TestDidStartNavigationInfo() = default;
TestDidFinishNavigationInfo::TestDidFinishNavigationInfo() {}
TestDidFinishNavigationInfo::~TestDidFinishNavigationInfo() = default;
}  // namespace web
