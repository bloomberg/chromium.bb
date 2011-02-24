// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/cros/login_library.h"

class FilePath;

namespace base {
class RSAPrivateKey;
}

namespace chromeos {

class OwnerKeyUtilsTest;

class OwnerKeyUtils : public base::RefCounted<OwnerKeyUtils> {
 public:
  class Factory {
   public:
    virtual OwnerKeyUtils* CreateOwnerKeyUtils() = 0;
  };

  OwnerKeyUtils();

  // Sets the factory used by the static method Create to create an
  // OwnerKeyUtils.  OwnerKeyUtils does not take ownership of
  // |factory|. A value of NULL results in an OwnerKeyUtils being
  // created directly.
#if defined(UNIT_TEST)
  static void set_factory(Factory* factory) { factory_ = factory; }
#endif

  // Creates an OwnerKeyUtils, ownership returns to the caller. If there is no
  // Factory (the default) this creates and returns a new OwnerKeyUtils.
  static OwnerKeyUtils* Create();

  // Assumes that the file at |key_file| exists.
  // Upon success, returns true and populates |output|.  False on failure.
  virtual bool ImportPublicKey(const FilePath& key_file,
                               std::vector<uint8>* output) = 0;

  // Verfiy that |signature| is a Sha1-with-RSA signature over |data| with
  // |public_key|
  // Returns true if so, false on bad signature or other error.
  virtual bool Verify(const std::string& data,
                      const std::vector<uint8> signature,
                      const std::vector<uint8> public_key) = 0;

  // Sign |data| with |key| using Sha1 with RSA.  If successful, return true
  // and populate |OUT_signature|.
  virtual bool Sign(const std::string& data,
                    std::vector<uint8>* OUT_signature,
                    base::RSAPrivateKey* key) = 0;

  // Looks for the private key associated with |key| in the default slot,
  // and returns it if it can be found.  Returns NULL otherwise.
  // Caller takes ownership.
  virtual base::RSAPrivateKey* FindPrivateKey(
      const std::vector<uint8>& key) = 0;

  virtual FilePath GetOwnerKeyFilePath() = 0;

 protected:
  virtual ~OwnerKeyUtils();

  // DER encodes public half of |pair| and writes it out to |key_file|.
  // The blob on disk is a DER-encoded X509 SubjectPublicKeyInfo object.
  // Returns false on error.
  virtual bool ExportPublicKeyToFile(base::RSAPrivateKey* pair,
                                     const FilePath& key_file) = 0;

 private:
  friend class base::RefCounted<OwnerKeyUtils>;
  static Factory* factory_;

  FRIEND_TEST_ALL_PREFIXES(OwnerKeyUtilsTest, ExportImportPublicKey);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
