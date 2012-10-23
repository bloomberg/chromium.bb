// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_POLICY_H_
#define CONTENT_COMMON_SANDBOX_POLICY_H_

namespace sandbox {
class BrokerServices;
class TargetServices;
}

namespace content {

bool InitBrokerServices(sandbox::BrokerServices* broker_services);

bool InitTargetServices(sandbox::TargetServices* target_services);

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_POLICY_H_
