// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OpenSSL multi-threading initialization

#ifndef CHROME_BROWSER_SYNC_ENGINE_NET_OPENSSL_INIT_H_
#define CHROME_BROWSER_SYNC_ENGINE_NET_OPENSSL_INIT_H_

namespace browser_sync {

// Initializes the OpenSSL multithreading callbacks. Returns false on failure.
void InitOpenSslMultithreading();

// Cleans up the OpenSSL multithreading callbacks.
void CleanupOpenSslMultithreading();

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_NET_OPENSSL_INIT_H_
