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
  // Does nothing.
  virtual void Reseed(const void* additional_entropy,
                      size_t entropy_len) OVERRIDE;

  // ChangeValue increments |increment_|. This causes the value returned by
  // |RandUint64| and the byte that |RandBytes| fills with, to change.
  void ChangeValue();

 private:
  uint8 increment_;
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_
