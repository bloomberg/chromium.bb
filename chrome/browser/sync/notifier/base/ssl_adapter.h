// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_SSL_ADAPTER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_SSL_ADAPTER_H_

namespace talk_base {
class AsyncSocket;
class SSLAdapter;
}  // namespace talk_base

namespace notifier {

// Wraps the given socket in a platform-dependent SSLAdapter
// implementation.
talk_base::SSLAdapter* CreateSSLAdapter(talk_base::AsyncSocket* socket);

// Utility template class that overrides CreateSSLAdapter() to use the
// above function.
template <class SocketFactory>
class SSLAdapterSocketFactory : public SocketFactory {
 public:
  virtual talk_base::SSLAdapter* CreateSSLAdapter(
      talk_base::AsyncSocket* socket) {
    return ::notifier::CreateSSLAdapter(socket);
  }
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_SSL_ADAPTER_H_

