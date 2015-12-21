// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encryptor.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/sys_byteorder.h"

namespace crypto {

/////////////////////////////////////////////////////////////////////////////
// Encyptor::Counter Implementation.
Encryptor::Counter::Counter(const base::StringPiece& counter) {
  CHECK(sizeof(counter_) == counter.length());

  memcpy(&counter_, counter.data(), sizeof(counter_));
}

Encryptor::Counter::~Counter() {
}

bool Encryptor::Counter::Increment() {
  uint64_t low_num = base::NetToHost64(counter_.components64[1]);
  uint64_t new_low_num = low_num + 1;
  counter_.components64[1] = base::HostToNet64(new_low_num);

  // If overflow occured then increment the most significant component.
  if (new_low_num < low_num) {
    counter_.components64[0] =
        base::HostToNet64(base::NetToHost64(counter_.components64[0]) + 1);
  }

  // TODO(hclam): Return false if counter value overflows.
  return true;
}

void Encryptor::Counter::Write(void* buf) {
  uint8_t* buf_ptr = reinterpret_cast<uint8_t*>(buf);
  memcpy(buf_ptr, &counter_, sizeof(counter_));
}

size_t Encryptor::Counter::GetLengthInBytes() const {
  return sizeof(counter_);
}

/////////////////////////////////////////////////////////////////////////////
// Partial Encryptor Implementation.

bool Encryptor::SetCounter(const base::StringPiece& counter) {
  if (mode_ != CTR)
    return false;
  if (counter.length() != 16u)
    return false;

  counter_.reset(new Counter(counter));
  return true;
}

bool Encryptor::GenerateCounterMask(size_t plaintext_len,
                                    uint8_t* mask,
                                    size_t* mask_len) {
  DCHECK_EQ(CTR, mode_);
  CHECK(mask);
  CHECK(mask_len);

  const size_t kBlockLength = counter_->GetLengthInBytes();
  size_t blocks = (plaintext_len + kBlockLength - 1) / kBlockLength;
  CHECK(blocks);

  *mask_len = blocks * kBlockLength;

  for (size_t i = 0; i < blocks; ++i) {
    counter_->Write(mask);
    mask += kBlockLength;

    bool ret = counter_->Increment();
    if (!ret)
      return false;
  }
  return true;
}

void Encryptor::MaskMessage(const void* plaintext,
                            size_t plaintext_len,
                            const void* mask,
                            void* ciphertext) const {
  DCHECK_EQ(CTR, mode_);
  const uint8_t* plaintext_ptr = reinterpret_cast<const uint8_t*>(plaintext);
  const uint8_t* mask_ptr = reinterpret_cast<const uint8_t*>(mask);
  uint8_t* ciphertext_ptr = reinterpret_cast<uint8_t*>(ciphertext);

  for (size_t i = 0; i < plaintext_len; ++i)
    ciphertext_ptr[i] = plaintext_ptr[i] ^ mask_ptr[i];
}

}  // namespace crypto
