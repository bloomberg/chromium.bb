// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_H_
#define NET_SSL_SSL_PLATFORM_KEY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {

class SSLPrivateKey;
class X509Certificate;

// Looks up the private key from the platform key store corresponding to
// |certificate|'s public key and returns an SSLPrivateKey which offloads
// signing operations to |task_runner|. |task_runner| is a SequencedTaskRunner
// to allow for buggy third-party smartcard drivers.
scoped_ptr<SSLPrivateKey> FetchClientCertPrivateKey(
    X509Certificate* certificate,
    scoped_refptr<base::SequencedTaskRunner> task_runner);

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_H_
