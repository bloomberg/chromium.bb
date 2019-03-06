// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_account_fetcher_service.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "ui/gfx/image/image_unittest_util.h"

FakeAccountFetcherService::FakeAccountFetcherService() {}

void FakeAccountFetcherService::StartFetchingUserInfo(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}

#if defined(OS_ANDROID)
void FakeAccountFetcherService::StartFetchingChildInfo(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}
#endif

TestImageDecoder::TestImageDecoder() = default;

TestImageDecoder::~TestImageDecoder() = default;

void TestImageDecoder::DecodeImage(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    const image_fetcher::ImageDecodedCallback& callback) {
  callback.Run(image_data.empty() ? gfx::Image()
                                  : gfx::test::CreateImage(64, 64));
}
