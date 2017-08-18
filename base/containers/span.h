// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SPAN_H_
#define BASE_SPAN_H_

#include <stddef.h>

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

namespace base {

template <typename T>
class Span;

namespace internal {

template <typename T>
struct IsSpanImpl : std::false_type {};

template <typename T>
struct IsSpanImpl<Span<T>> : std::true_type {};

template <typename T>
using IsSpan = IsSpanImpl<std::decay_t<T>>;

template <typename T>
struct IsStdArrayImpl : std::false_type {};

template <typename T, size_t N>
struct IsStdArrayImpl<std::array<T, N>> : std::true_type {};

template <typename T>
using IsStdArray = IsStdArrayImpl<std::decay_t<T>>;

template <typename From, typename To>
using IsLegalSpanConversion = std::is_convertible<From*, To*>;

template <typename Container, typename T>
using ContainerHasConvertibleData = IsLegalSpanConversion<
    std::remove_pointer_t<decltype(std::declval<Container>().data())>,
    T>;
template <typename Container>
using ContainerHasIntegralSize =
    std::is_integral<decltype(std::declval<Container>().size())>;

template <typename From, typename To>
using EnableIfLegalSpanConversion =
    std::enable_if_t<IsLegalSpanConversion<From, To>::value>;

// SFINAE check if Container can be converted to a Span<T>. Note that the
// implementation details of this check differ slightly from the requirements in
// the working group proposal: in particular, the proposal also requires that
// the container conversion constructor participate in overload resolution only
// if two additional conditions are true:
//
//   1. Container implements operator[].
//   2. Container::value_type matches remove_const_t<element_type>.
//
// The requirements are relaxed slightly here: in particular, not requiring (2)
// means that an immutable Span can be easily constructed from a mutable
// container.
template <typename Container, typename T>
using EnableIfSpanCompatibleContainer =
    std::enable_if_t<!internal::IsSpan<Container>::value &&
                     !internal::IsStdArray<Container>::value &&
                     ContainerHasConvertibleData<Container, T>::value &&
                     ContainerHasIntegralSize<Container>::value>;

}  // namespace internal

// A Span is a value type that represents an array of elements of type T.  It
// consists of a pointer to memory with an associated size. A Span does not own
// the underlying memory, so care must be taken to ensure that a Span does not
// outlive the backing store.
//
// Span is somewhat analogous to StringPiece, but with arbitrary element types,
// allowing mutation if T is non-const.
//
// Span is implicitly convertible from C++ arrays, as well as most [1]
// container-like types that provide a data() and size() method (such as
// std::vector<T>). A mutable Span<T> can also be implicitly converted to an
// immutable Span<const T>.
//
// Consider using a Span for functions that take a data pointer and size
// parameter: it allows the function to still act on an array-like type, while
// allowing the caller code to be a bit more concise.
//
// Without span:
//   // std::string HexEncode(uint8_t* data, size_t size);
//   std::vector<uint8_t> data_buffer = GenerateData();
//   std::string r = HexEncode(data_buffer.data(), data_buffer.size());
//
// With span:
//   // std::string HexEncode(base::Span<uint8_t> data);
//   std::vector<uint8_t> data_buffer = GenerateData();
//   std::string r = HexEncode(data_buffer);
//
// Span differs from the C++ working group proposal in a number of ways:
// - Span does not define the |element_type| and |index_type| type aliases.
// - Span does not explicitly default the copy/move constructor/assignment
//   operators, since MSVC currently complains about constexpr functions that
//   aren't marked const.
// - Span does not define operator().
// - Span does not define bytes(), size_bytes(), as_bytes(), as_mutable_bytes()
//   for working with spans as a sequence of bytes.
// - Span has no extent template parameter.
// - Span has no conversion constructors from std::unique_ptr or
//   std::shared_ptr.
// - Span has no reverse iterators.
// - Span does not define relation operators other than == and !=.
//
// [1] std::array<T, N> has its own constructor overload which is not
//     implemented.
//
// TODO(https://crbug.com/754077): Document differences from the working group
// proposal: http://open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0122r1.pdf.
template <typename T>
class Span {
 public:
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using iterator = T*;
  using const_iterator = const T*;
  // TODO(dcheng): What about reverse iterators?

  // Span constructors, copy, assignment, and destructor
  constexpr Span() noexcept : data_(nullptr), size_(0) {}
  constexpr Span(T* data, size_t size) noexcept : data_(data), size_(size) {}
  // TODO(dcheng): Implement construction from a |begin| and |end| pointer.
  template <size_t N>
  constexpr Span(T (&array)[N]) noexcept : Span(array, N) {}
  // TODO(dcheng): Implement construction from std::array.
  // Conversion from a container that provides |T* data()| and |integral_type
  // size()|.
  // TODO(dcheng): Should this be explicit for mutable spans?
  template <typename Container,
            typename = internal::EnableIfSpanCompatibleContainer<Container, T>>
  constexpr Span(Container& container)
      : Span(container.data(), container.size()) {}
  // Disallow conversion from a container going out of scope, since the
  // resulting span will just point to dangling memory. Note that this won't
  // catch everything. For example, the following will still compile:
  //   Span<int> ReturnsDanglingSpan() {
  //     std::vector<int> v = GetVector();
  //     return v;
  //   }
  template <typename Container,
            typename = internal::EnableIfSpanCompatibleContainer<Container, T>>
  Span(const Container&&) = delete;
  ~Span() noexcept = default;
  // Conversions from spans of compatible types: this allows a Span<T> to be
  // seamlessly used as a Span<const T>, but not the other way around.
  template <typename U, typename = internal::EnableIfLegalSpanConversion<U, T>>
  constexpr Span(const Span<U>& other) : Span(other.data(), other.size()) {}
  template <typename U, typename = internal::EnableIfLegalSpanConversion<U, T>>
  constexpr Span(Span<U>&& other) : Span(other.data(), other.size()) {}

  // Span subviews
  constexpr Span subspan(size_t pos, size_t count) const {
    // Note: ideally this would DCHECK, but it requires fairly horrible
    // contortions.
    return Span(data_ + pos, count);
  }

  // Span observers
  constexpr size_t size() const noexcept { return size_; }

  // Span element access
  constexpr T& operator[](size_t index) const noexcept { return data_[index]; }
  constexpr T* data() const noexcept { return data_; }

  // Span iterator support
  iterator begin() const noexcept { return data_; }
  iterator end() const noexcept { return data_ + size_; }

  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

 private:
  T* data_;
  size_t size_;
};

// Relational operators. Equality is a element-wise comparison.
template <typename T>
constexpr bool operator==(const Span<T>& lhs, const Span<T>& rhs) noexcept {
  return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template <typename T>
constexpr bool operator!=(const Span<T>& lhs, const Span<T>& rhs) noexcept {
  return !(lhs == rhs);
}

// TODO(dcheng): Implement other relational operators.

// Type-deducing helpers for constructing a Span.
template <typename T>
constexpr Span<T> MakeSpan(T* data, size_t size) noexcept {
  return Span<T>(data, size);
}

template <typename T, size_t N>
constexpr Span<T> MakeSpan(T (&array)[N]) noexcept {
  return Span<T>(array);
}

template <typename Container,
          typename T = typename Container::value_type,
          typename = internal::EnableIfSpanCompatibleContainer<Container, T>>
constexpr Span<T> MakeSpan(Container& container) {
  return Span<T>(container);
}

// TODO(dcheng): Should there be a const Container&& overload to try to prevent
// MakeSpan from binding to rvalue containers?

}  // namespace base

#endif  // BASE_SPAN_H_
