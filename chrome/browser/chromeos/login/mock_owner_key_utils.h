// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNER_KEY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNER_KEY_UTILS_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/crypto/rsa_private_key.h"
#include "base/file_path.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/owner_key_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::RSAPrivateKey;

namespace chromeos {

class MockKeyUtils : public OwnerKeyUtils {
 public:
  MockKeyUtils() {}
  MOCK_METHOD0(GenerateKeyPair, RSAPrivateKey*());
  MOCK_METHOD2(ExportPublicKeyViaDbus, bool(RSAPrivateKey* pair,
                                            LoginLibrary::Delegate*));
  MOCK_METHOD2(ExportPublicKeyToFile, bool(RSAPrivateKey* pair,
                                           const FilePath& key_file));
  MOCK_METHOD2(ImportPublicKey, bool(const FilePath& key_file,
                                     std::vector<uint8>* output));
  MOCK_METHOD3(Verify, bool(const std::string& data,
                            const std::vector<uint8> signature,
                            const std::vector<uint8> public_key));
  MOCK_METHOD3(Sign, bool(const std::string& data,
                          std::vector<uint8>* OUT_signature,
                          base::RSAPrivateKey* key));
  MOCK_METHOD1(FindPrivateKey, RSAPrivateKey*(const std::vector<uint8>& key));
  MOCK_METHOD0(GetOwnerKeyFilePath, FilePath());

  // To simulate doing a LoginLibrary::SetOwnerKey call
  static void SetOwnerKeyCallback(LoginLibrary::Delegate* callback,
                                  bool value) {
    callback->OnComplete(value);
  }

  static bool ExportPublicKeyViaDbusWin(RSAPrivateKey* key,
                                        LoginLibrary::Delegate* d) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&SetOwnerKeyCallback, d, true));
    return true;
  }

  static bool ExportPublicKeyViaDbusFail(RSAPrivateKey* key,
                                         LoginLibrary::Delegate* d) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&SetOwnerKeyCallback, d, false));
    return false;
  }

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
