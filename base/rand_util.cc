// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <algorithm>
#include <ctime>
#include <limits>
#include <random>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_local.h"

namespace {

class PrngThreadLocalStorage;

base::LazyInstance<
    base::ThreadLocalUniquePointer<PrngThreadLocalStorage>>::Leaky prng_tls =
    LAZY_INSTANCE_INITIALIZER;

// PrngThreadLocalStorage stores a pointer to the thread's seeded random number
// generator in the thread's local storage. Note that at most one generator can
// be bound to each thread at a time.
// Example  Usage:
//   prng =  PrngThreadLocalStorage::GetGenerator();
//   prng -> GetRandomInteger(0,20);
class PrngThreadLocalStorage {
 public:
  PrngThreadLocalStorage() : prng_(time(nullptr)){};

  ~PrngThreadLocalStorage() = default;

  // Returns true if a pseudo-random number generator has been assigned to
  // the current thread.
  static bool IsSet() { return prng_tls.Get().Get(); }

  // Returns the random generator bound to the current thread. If no such
  // generator exist, it creates an instance and binds it to the thread.
  static PrngThreadLocalStorage* GetGenerator() {
    PrngThreadLocalStorage* instance = prng_tls.Get().Get();
    if (!instance) {
      prng_tls.Get().Set(std::make_unique<PrngThreadLocalStorage>());
      instance = prng_tls.Get().Get();
    }
    return instance;
  }

  // Returns a uniformly distributed random integer in the range [start,end].
  int GetRandomInteger(int start, int end) {
    std::uniform_int_distribution<> distribution(start, end);
    return distribution(prng_);
  }

 private:
  std::mt19937 prng_;

  DISALLOW_COPY_AND_ASSIGN(PrngThreadLocalStorage);
};

}  // namespace

namespace base {

uint64_t RandUint64() {
  uint64_t number;
  RandBytes(&number, sizeof(number));
  return number;
}

int RandInt(int min, int max) {
  DCHECK_LE(min, max);

  PrngThreadLocalStorage* prng = PrngThreadLocalStorage::GetGenerator();
  int result = prng->GetRandomInteger(min, max);

  DCHECK_GE(result, min);
  DCHECK_LE(result, max);
  return result;
}

double RandDouble() {
  return BitsToOpenEndedUnitInterval(base::RandUint64());
}

double BitsToOpenEndedUnitInterval(uint64_t bits) {
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 doubles, the mantissa
  // is expected to accommodate 53 bits.

  static_assert(std::numeric_limits<double>::radix == 2,
                "otherwise use scalbn");
  static const int kBits = std::numeric_limits<double>::digits;
  uint64_t random_bits = bits & ((UINT64_C(1) << kBits) - 1);
  double result = ldexp(static_cast<double>(random_bits), -1 * kBits);
  DCHECK_GE(result, 0.0);
  DCHECK_LT(result, 1.0);
  return result;
}

uint64_t RandGenerator(uint64_t range) {
  DCHECK_GT(range, 0u);
  // We must discard random results above this number, as they would
  // make the random generator non-uniform (consider e.g. if
  // MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
  // as likely as a result of 3 or 4).
  uint64_t max_acceptable_value =
      (std::numeric_limits<uint64_t>::max() / range) * range - 1;

  uint64_t value;
  do {
    value = base::RandUint64();
  } while (value > max_acceptable_value);

  return value % range;
}

std::string RandBytesAsString(size_t length) {
  DCHECK_GT(length, 0u);
  std::string result;
  RandBytes(WriteInto(&result, length + 1), length);
  return result;
}

}  // namespace base
