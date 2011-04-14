// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNER_KEY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNER_KEY_UTILS_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "crypto/rsa_private_key.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/owner_key_utils.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class MockKeyUtils : public OwnerKeyUtils {
 public:
  MockKeyUtils() {}
  MOCK_METHOD2(ImportPublicKey, bool(const FilePath& key_file,
                                     std::vector<uint8>* output));
  MOCK_METHOD3(Verify, bool(const std::string& data,
                            const std::vector<uint8> signature,
                            const std::vector<uint8> public_key));
  MOCK_METHOD3(Sign, bool(const std::string& data,
                          std::vector<uint8>* OUT_signature,
                          crypto::RSAPrivateKey* key));
  MOCK_METHOD1(FindPrivateKey,
               crypto::RSAPrivateKey*(const std::vector<uint8>& key));
  MOCK_METHOD0(GetOwnerKeyFilePath, FilePath());
  MOCK_METHOD2(ExportPublicKeyToFile, bool(crypto::RSAPrivateKey* pair,
                                           const FilePath& key_file));
 protected:
  virtual ~MockKeyUtils() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyUtils);
};

class MockInjector : public OwnerKeyUtils::Factory {
 public:
  // Takes ownership of |mock|.
  explicit MockInjector(MockKeyUtils* mock) : transient_(mock) {
  }

  virtual ~MockInjector() {}

  // If this is called, its caller takes ownership of |transient_|.
  // If it's never called, |transient_| remains our problem.
  OwnerKeyUtils* CreateOwnerKeyUtils() {
    return transient_.get();
  }

 private:
  scoped_refptr<MockKeyUtils> transient_;
  DISALLOW_COPY_AND_ASSIGN(MockInjector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNER_KEY_UTILS_H_
