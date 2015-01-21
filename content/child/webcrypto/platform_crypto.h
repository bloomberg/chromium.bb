// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_
#define CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_

#include <stdint.h>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

// The definitions for these methods lives in either nss/ or openssl/
namespace content {

namespace webcrypto {

class AlgorithmImplementation;

void PlatformInit();

scoped_ptr<blink::WebCryptoDigestor> CreatePlatformDigestor(
    blink::WebCryptoAlgorithmId algorithm);

AlgorithmImplementation* CreatePlatformShaImplementation();
AlgorithmImplementation* CreatePlatformAesCbcImplementation();
AlgorithmImplementation* CreatePlatformAesCtrImplementation();
AlgorithmImplementation* CreatePlatformAesGcmImplementation();
AlgorithmImplementation* CreatePlatformAesKwImplementation();
AlgorithmImplementation* CreatePlatformHmacImplementation();
AlgorithmImplementation* CreatePlatformRsaOaepImplementation();
AlgorithmImplementation* CreatePlatformRsaSsaImplementation();
AlgorithmImplementation* CreatePlatformRsaPssImplementation();
AlgorithmImplementation* CreatePlatformEcdsaImplementation();
AlgorithmImplementation* CreatePlatformEcdhImplementation();
AlgorithmImplementation* CreatePlatformHkdfImplementation();
AlgorithmImplementation* CreatePlatformPbkdf2Implementation();

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_
