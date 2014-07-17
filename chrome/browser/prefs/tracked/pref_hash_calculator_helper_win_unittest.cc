// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/tracked/pref_hash_calculator_helper.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/music_manager_private/device_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "crypto/hmac.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace {

// extensions::api::DeviceId::GetDeviceId() signs the extension_id in
// GetRawDeviceIdCallback to get the final device_id, our code replicating this
// id needs to do the same thing.
const char kFakeExtensionId[] = "foo";

// Sets |media_device_id_out| to |media_device_id_in| and unblocks the original
// binder by calling |unblock_callback|.
void SetMediaDeviceIdAndUnblock(const base::Closure& unblock_callback,
                                std::string* media_device_id_out,
                                const std::string& media_device_id_in) {
  if (media_device_id_in.empty())
    LOG(WARNING) << "Media device ID is empty.";
  *media_device_id_out = media_device_id_in;
  unblock_callback.Run();
}

// Returns the device id from extensions::api::DeviceId::GetDeviceId()
// synchronously.
std::string GetMediaDeviceIdSynchronously() {
  std::string media_device_id;
  const scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  extensions::api::DeviceId::GetDeviceId(kFakeExtensionId,
                               base::Bind(&SetMediaDeviceIdAndUnblock,
                                          runner->QuitClosure(),
                                          base::Unretained(&media_device_id)));
  // Run the message loop until SetMediaDeviceIdAndUnblock() calls the
  // QuitClosure when done.
  runner->Run();
  return media_device_id;
}

// Copy functionality from GetRawDeviceIdCallback() to get the final device id
// from the |raw_device_id|. Signing |kExtension| with an HMAC-SHA256 using
// |raw_device_id| as its key.
std::string GetDeviceIdFromRawDeviceId(const std::string& raw_device_id) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8> digest(digest_length);
  bool result = hmac.Init(raw_device_id) &&
      hmac.Sign(kFakeExtensionId, &digest[0], digest.size());
  if (!result) {
    ADD_FAILURE();
    return std::string();
  }
  return StringToLowerASCII(base::HexEncode(digest.data(), digest.size()));
}

#if defined(ENABLE_RLZ)
std::string GetLegacyIdBasedOnRlzId() {
  std::string rlz_machine_id;
  rlz_lib::GetMachineId(&rlz_machine_id);
  EXPECT_FALSE(rlz_machine_id.empty());

  const std::string raw_legacy_device_id(GetLegacyDeviceId(rlz_machine_id));

  if (raw_legacy_device_id.empty()) {
    LOG(WARNING) << "Raw legacy device ID based on RLZ ID is empty.";
    return std::string();
  }

  const std::string legacy_device_id(
      GetDeviceIdFromRawDeviceId(raw_legacy_device_id));
  EXPECT_FALSE(legacy_device_id.empty());

  return legacy_device_id;
}
#endif  // ENABLE_RLZ

// Simulate browser threads (required by extensions::api::DeviceId) off of the
// main message loop.
class PrefHashCalculatorHelperTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle;
};

}  // namespace

// The implementation for the legacy ID on Windows is copied from
// the M33 version of extensions::api::DeviceId::GetDeviceId(). We no longer
// depend on it as of M34, but should make sure that we are generating the same
// results in the mean time (it will be okay for the extension API's
// implementation to diverge on M34+ and this test can be removed once M34 ships
// to stable).
#if defined(ENABLE_RLZ)
TEST_F(PrefHashCalculatorHelperTest, ResultMatchesMediaId) {
  EXPECT_EQ(GetMediaDeviceIdSynchronously(), GetLegacyIdBasedOnRlzId());
}
#endif  // ENABLE_RLZ

TEST_F(PrefHashCalculatorHelperTest, MediaIdIsDeterministic) {
  EXPECT_EQ(GetMediaDeviceIdSynchronously(), GetMediaDeviceIdSynchronously());
}

#if defined(ENABLE_RLZ)
TEST_F(PrefHashCalculatorHelperTest, RlzBasedIdIsDeterministic) {
  EXPECT_EQ(GetLegacyIdBasedOnRlzId(), GetLegacyIdBasedOnRlzId());
}
#endif  // ENABLE_RLZ
