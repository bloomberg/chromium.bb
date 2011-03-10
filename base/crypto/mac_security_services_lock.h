// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_MAC_SECURITY_SERVICES_LOCK_H_
#define BASE_CRYPTO_MAC_SECURITY_SERVICES_LOCK_H_
#pragma once

namespace base {

class Lock;

// The Mac OS X certificate and key management wrappers over CSSM are not
// thread-safe. In particular, code that accesses the CSSM database is
// problematic.
//
// http://developer.apple.com/mac/library/documentation/Security/Reference/certifkeytrustservices/Reference/reference.html
Lock& GetMacSecurityServicesLock();

}  // namespace base

#endif  // BASE_CRYPTO_MAC_SECURITY_SERVICES_LOCK_H_
