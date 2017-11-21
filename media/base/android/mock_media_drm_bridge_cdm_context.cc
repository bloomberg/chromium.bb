// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/mock_media_drm_bridge_cdm_context.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace media {

MockMediaDrmBridgeCdmContext::MockMediaDrmBridgeCdmContext(int cdm_id) {
  // Provide some sane defaults.
  ON_CALL(*this, GetCdmId()).WillByDefault(Return(cdm_id));
  ON_CALL(*this, RegisterPlayer(_, _))
      .WillByDefault(DoAll(SaveArg<0>(&new_key_cb), SaveArg<1>(&cdm_unset_cb),
                           Return(kRegistrationId)));
  ON_CALL(*this, SetMediaCryptoReadyCB(_))
      .WillByDefault(SaveArg<0>(&media_crypto_ready_cb));

  // Don't set any expectation on the number of correct calls to
  // UnregisterPlayer, but expect no calls with the wrong registration id.
  EXPECT_CALL(*this, UnregisterPlayer(Not(kRegistrationId))).Times(0);
}

MockMediaDrmBridgeCdmContext::~MockMediaDrmBridgeCdmContext() {}

}  // namespace media
