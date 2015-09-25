// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SHA256_LEGACY_SUPPORT_WIN_H_
#define NET_CERT_SHA256_LEGACY_SUPPORT_WIN_H_

#include <windows.h>

#include "base/strings/string_piece.h"
#include "crypto/wincrypt_shim.h"
#include "net/base/net_export.h"

namespace net {

namespace sha256_interception {

typedef BOOL (WINAPI* CryptVerifyCertificateSignatureExFunc)(
    HCRYPTPROV_LEGACY provider,
    DWORD encoding_type,
    DWORD subject_type,
    void* subject_data,
    DWORD issuer_type,
    void* issuer_data,
    DWORD flags,
    void* extra);

// Interception function meant to be called whenever
// CryptVerifyCertificateSignatureEx is called. Note that the calling
// conventions do not match, as the caller is expected to ensure that their
// interposed function handles the calling conventions and provides a pointer
// to the original CryptVerifyCertificateSignatureEx (e.g. to handle parameters
// and keys that are not supported).
NET_EXPORT BOOL CryptVerifyCertificateSignatureExHook(
    CryptVerifyCertificateSignatureExFunc original_func,
    HCRYPTPROV_LEGACY provider,
    DWORD encoding_type,
    DWORD subject_type,
    void* subject_data,
    DWORD issuer_type,
    void* issuer_data,
    DWORD flags,
    void* extra);

}  // namespace sha256_interception

}  // namespace net

#endif  // NET_CERT_SHA256_LEGACY_SUPPORT_WIN_H_
