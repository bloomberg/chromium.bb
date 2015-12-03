// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.
#ifndef DEFINE_CONTEXT_CONFIG
#error "DEFINE_CONTEXT_CONFIG should be defined before including this file"
#endif
// See CronetEngine.Builder class in CronetEngine.java for description of these
// parameters.
DEFINE_CONTEXT_CONFIG(USER_AGENT)
DEFINE_CONTEXT_CONFIG(STORAGE_PATH)
DEFINE_CONTEXT_CONFIG(ENABLE_LEGACY_MODE)
DEFINE_CONTEXT_CONFIG(NATIVE_LIBRARY_NAME)
DEFINE_CONTEXT_CONFIG(ENABLE_QUIC)
DEFINE_CONTEXT_CONFIG(ENABLE_SPDY)
DEFINE_CONTEXT_CONFIG(ENABLE_SDCH)
DEFINE_CONTEXT_CONFIG(DATA_REDUCTION_PROXY_KEY)
DEFINE_CONTEXT_CONFIG(DATA_REDUCTION_PRIMARY_PROXY)
DEFINE_CONTEXT_CONFIG(DATA_REDUCTION_FALLBACK_PROXY)
DEFINE_CONTEXT_CONFIG(DATA_REDUCTION_SECURE_PROXY_CHECK_URL)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE_MAX_SIZE)

DEFINE_CONTEXT_CONFIG(HTTP_CACHE_DISABLED)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE_DISK)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE_MEMORY)
DEFINE_CONTEXT_CONFIG(LOAD_DISABLE_CACHE)

DEFINE_CONTEXT_CONFIG(QUIC_HINTS)
DEFINE_CONTEXT_CONFIG(QUIC_HINT_HOST)
DEFINE_CONTEXT_CONFIG(QUIC_HINT_PORT)
DEFINE_CONTEXT_CONFIG(QUIC_HINT_ALT_PORT)

DEFINE_CONTEXT_CONFIG(EXPERIMENTAL_OPTIONS)

// The list of PKP info for multiple hosts. Each member of the list pins a
// single host and contains the following nested JSON elements: PKP_HOST,
// PKP_PIN_HASHES, PKP_INCLUDE_SUBDOMAINS and PKP_EXPIRATION_DATE.
DEFINE_CONTEXT_CONFIG(PKP_LIST)
// Name of the host to which public keys should be pinned. See PKP_LIST.
DEFINE_CONTEXT_CONFIG(PKP_HOST)
// The list of PKP pins. Each pin is BASE64 encoded SHA-256 cryptographic
// hash of DER-encoded ASN.1 representation of Subject Public Key Info (SPKI)
// of the host X.509 certificate. The pins are prepended with "sha256/" prefix.
// See PKP_LIST.
DEFINE_CONTEXT_CONFIG(PKP_PIN_HASHES)
// Indicates whether the pinning policy should be applied to subdomains of
// PKP_HOST. See PKP_LIST.
DEFINE_CONTEXT_CONFIG(PKP_INCLUDE_SUBDOMAINS)
// Specifies the expiration date for the pins. See PKP_LIST.
DEFINE_CONTEXT_CONFIG(PKP_EXPIRATION_DATE)

// For Testing.
DEFINE_CONTEXT_CONFIG(MOCK_CERT_VERIFIER)
