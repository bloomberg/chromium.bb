// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_
#define NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_

#include "base/compiler_specific.h"
#include "net/quic/crypto/quic_random.h"

namespace net {

class MockRandom : public QuicRandom {
 public:
  MockRandom();

  // QuicRandom:
  // Fills the |data| buffer with a repeating byte, initially 'r'.
  virtual void RandBytes(void* data, size_t len) OVERRIDE;
  // Returns 0xDEADBEEF + the current increment.
  virtual uint64 RandUint64() OVERRIDE;
  // Returns false.
  virtual bool RandBool() OVERRIDE;
  // Reseed advances |increment_| which causes the value returned by
  // |RandUint64| to increment and the byte that |RandBytes| fills with, to
  // advance.
  virtual void Reseed(const void* additional_entropy,
                      size_t entropy_len) OVERRIDE;

 private:
  uint8 increment_;
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_
