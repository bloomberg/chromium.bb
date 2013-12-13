// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SIGNATURE_VALIDATOR_WIN_H_
#define CHROME_APP_SIGNATURE_VALIDATOR_WIN_H_

#include <string>
#include <vector>

namespace base {
class FilePath;
}

// Verifies that |signed_file| has a valid signature from a trusted software
// publisher. The signing certificate must be valid for code signing, and must
// be issued by a trusted certificate authority (e.g., VeriSign, Inc).
bool VerifyAuthenticodeSignature(const base::FilePath& signed_file);

// Tries to verify the signer by matching the subject name of the
// certificate to |subject_name| and the hash of the public key to
// |expected_hashes|. The cert must be current. If matched, returns true
// otherwise returns false.
bool VerifySignerIsGoogle(const base::FilePath& signed_file,
                          const std::string& subject_name,
                          const std::vector<std::string>& expected_hashes);

#endif  // CHROME_APP_SIGNATURE_VALIDATOR_WIN_H_
