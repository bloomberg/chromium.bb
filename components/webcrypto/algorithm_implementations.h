// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATIONS_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATIONS_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

// The definitions for these functions live in the algorithms/ directory.
namespace webcrypto {

class AlgorithmImplementation;

scoped_ptr<blink::WebCryptoDigestor> CreateDigestorImplementation(
    blink::WebCryptoAlgorithmId algorithm);

scoped_ptr<AlgorithmImplementation> CreateShaImplementation();
scoped_ptr<AlgorithmImplementation> CreateAesCbcImplementation();
scoped_ptr<AlgorithmImplementation> CreateAesCtrImplementation();
scoped_ptr<AlgorithmImplementation> CreateAesGcmImplementation();
scoped_ptr<AlgorithmImplementation> CreateAesKwImplementation();
scoped_ptr<AlgorithmImplementation> CreateHmacImplementation();
scoped_ptr<AlgorithmImplementation> CreateRsaOaepImplementation();
scoped_ptr<AlgorithmImplementation> CreateRsaSsaImplementation();
scoped_ptr<AlgorithmImplementation> CreateRsaPssImplementation();
scoped_ptr<AlgorithmImplementation> CreateEcdsaImplementation();
scoped_ptr<AlgorithmImplementation> CreateEcdhImplementation();
scoped_ptr<AlgorithmImplementation> CreateHkdfImplementation();
scoped_ptr<AlgorithmImplementation> CreatePbkdf2Implementation();

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATIONS_H_
