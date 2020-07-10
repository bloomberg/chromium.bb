// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_HASH_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_HASH_UTIL_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/lib/hash_util.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {
namespace internal {

template <typename T>
size_t WTFHashCombine(size_t seed, const T& value) {
  // Based on proposal in:
  // http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2005/n1756.pdf
  //
  // TODO(tibell): We'd like to use WTF::DefaultHash instead of std::hash, but
  //     there is no general template specialization of DefaultHash for enums
  //     and there can't be an instance for bool.
  return seed ^ (std::hash<T>()(value) + (seed << 6) + (seed >> 2));
}

template <typename T, bool has_hash_method = HasHashMethod<T>::value>
struct WTFHashTraits;

template <typename T>
size_t WTFHash(size_t seed, const T& value);

template <typename T>
struct WTFHashTraits<T, true> {
  static size_t Hash(size_t seed, const T& value) { return value.Hash(seed); }
};

template <typename T>
struct WTFHashTraits<T, false> {
  static size_t Hash(size_t seed, const T& value) {
    return WTFHashCombine(seed, value);
  }
};

template <>
struct WTFHashTraits<WTF::String, false> {
  static size_t Hash(size_t seed, const WTF::String& value) {
    return HashCombine(seed, WTF::StringHash::GetHash(value));
  }
};

template <typename T>
size_t WTFHash(size_t seed, const T& value) {
  return WTFHashTraits<T>::Hash(seed, value);
}

template <typename T>
struct StructPtrHashFn {
  static unsigned GetHash(const StructPtr<T>& value) {
    return static_cast<unsigned>(value.Hash(kHashSeed));
  }
  static bool Equal(const StructPtr<T>& left, const StructPtr<T>& right) {
    return left.Equals(right);
  }
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

template <typename T>
struct InlinedStructPtrHashFn {
  static unsigned GetHash(const InlinedStructPtr<T>& value) {
    return static_cast<unsigned>(value.Hash(kHashSeed));
  }
  static bool Equal(const InlinedStructPtr<T>& left,
                    const InlinedStructPtr<T>& right) {
    return left.Equals(right);
  }
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace internal
}  // namespace mojo

namespace WTF {

template <typename T>
struct DefaultHash<mojo::StructPtr<T>> {
  using Hash = mojo::internal::StructPtrHashFn<T>;
};

template <typename T>
struct HashTraits<mojo::StructPtr<T>>
    : public GenericHashTraits<mojo::StructPtr<T>> {
  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const mojo::StructPtr<T>& value) {
    return value.is_null();
  }
  static void ConstructDeletedValue(mojo::StructPtr<T>& slot, bool) {
    mojo::internal::StructPtrWTFHelper<T>::ConstructDeletedValue(slot);
  }
  static bool IsDeletedValue(const mojo::StructPtr<T>& value) {
    return mojo::internal::StructPtrWTFHelper<T>::IsHashTableDeletedValue(
        value);
  }
};

template <typename T>
struct DefaultHash<mojo::InlinedStructPtr<T>> {
  using Hash = mojo::internal::InlinedStructPtrHashFn<T>;
};

template <typename T>
struct HashTraits<mojo::InlinedStructPtr<T>>
    : public GenericHashTraits<mojo::InlinedStructPtr<T>> {
  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const mojo::InlinedStructPtr<T>& value) {
    return value.is_null();
  }
  static void ConstructDeletedValue(mojo::InlinedStructPtr<T>& slot, bool) {
    mojo::internal::InlinedStructPtrWTFHelper<T>::ConstructDeletedValue(slot);
  }
  static bool IsDeletedValue(const mojo::InlinedStructPtr<T>& value) {
    return mojo::internal::InlinedStructPtrWTFHelper<
        T>::IsHashTableDeletedValue(value);
  }
};

}  // namespace WTF

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_HASH_UTIL_H_
