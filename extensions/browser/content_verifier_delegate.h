// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_

#include "url/gurl.h"

namespace base {
class Version;
}

namespace extensions {

class Extension;

// A pointer to the bytes of a public key, and the number of bytes.
struct ContentVerifierKey {
  const uint8* data;
  int size;

  ContentVerifierKey(const uint8* data, int size) {
    this->data = data;
    this->size = size;
  }
};

// This is an interface for clients that want to use a ContentVerifier.
class ContentVerifierDelegate {
 public:
  virtual ~ContentVerifierDelegate() {}

  // This should return true if the given extension should have its content
  // verified.
  virtual bool ShouldBeVerified(const Extension& extension) = 0;

  // Should return the public key to use for validating signatures via the two
  // out parameters. NOTE: the pointer returned *must* remain valid for the
  // lifetime of this object.
  virtual const ContentVerifierKey& PublicKey() = 0;

  // This should return a URL that can be used to fetch the
  // verified_contents.json containing signatures for the given extension
  // id/version pair.
  virtual GURL GetSignatureFetchUrl(const std::string& extension_id,
                                    const base::Version& version) = 0;

  // Called when the content verifier detects that a read of a file inside
  // an extension did not match its expected hash.
  virtual void VerifyFailed(const std::string& extension_id) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_
