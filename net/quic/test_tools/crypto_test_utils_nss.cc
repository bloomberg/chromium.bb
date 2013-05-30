// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "net/quic/crypto/channel_id.h"

using base::StringPiece;
using std::string;

namespace net {

namespace test {

// TODO(rtenneti): Implement NSS support ChannelIDSigner. Convert Sign() to be
// asynchronous using completion callback. After porting TestChannelIDSigner,
// implement real ChannelIDSigner.
class TestChannelIDSigner : public ChannelIDSigner {
 public:
  virtual ~TestChannelIDSigner() { }

  // ChannelIDSigner implementation.

  virtual bool Sign(const string& hostname,
                    StringPiece signed_data,
                    string* out_key,
                    string* out_signature) OVERRIDE {
    string key(HostnameToKey(hostname));

    *out_key = SerializeKey(key);
    if (out_key->empty()) {
      return false;
    }

    *out_signature = signed_data.as_string();

    return true;
  }

  static string KeyForHostname(const string& hostname) {
    string key(HostnameToKey(hostname));
    return SerializeKey(key);
  }

 private:
  static string HostnameToKey(const string& hostname) {
    string ret = hostname;
    return ret;
  }

  static string SerializeKey(string& key) {
    return string(key);
  }
};

// static
ChannelIDSigner* CryptoTestUtils::ChannelIDSignerForTesting() {
  return new TestChannelIDSigner();
}

// static
string CryptoTestUtils::ChannelIDKeyForHostname(const string& hostname) {
  return TestChannelIDSigner::KeyForHostname(hostname);
}

}  // namespace test

}  // namespace net
