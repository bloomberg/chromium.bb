// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_TEST_QUIC_TEST_SERVER_H_
#define COMPONENTS_CRONET_IOS_TEST_QUIC_TEST_SERVER_H_

namespace cronet {

bool StartQuicTestServer();

void ShutdownQuicTestServer();

}  // namespace cronet

#endif  // COMPONENTS_CRONET_IOS_TEST_QUIC_TEST_SERVER_H_
