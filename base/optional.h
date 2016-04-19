// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#include <type_traits>

#include "base/logging.h"
#include "base/memory/aligned_memory.h"

namespace base {

// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/in_place_t
struct in_place_t {};

// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/nullopt_t
struct nullopt_t {
  constexpr explicit nullopt_t(int) {}
};

// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/in_place
constexpr in_place_t in_place = {};

// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/nullopt
constexpr nullopt_t nullopt(0);

// base::Optional is a Chromium version of the C++17 optional class:
// std::optional documentation:
// http://en.cppreference.com/w/cpp/utility/optional
// Chromium documentation:
// https://chromium.googlesource.com/chromium/src/+/master/docs/optional.md
//
// These are the differences between the specification and the implementation:
// - The constructor and emplace method using initializer_list are not
//   implemented because 'initializer_list' is banned from Chromium.
// - Constructors do not use 'constexpr' as it is a C++14 extension.
// - 'constexpr' might be missing in some places for reasons specified locally.
// - No exceptions are thrown, because they are banned from Chromium.
// - All the non-members are in the 'base' namespace instead of 'std'.
template <typename T>
class Optional {
 public:
  constexpr Optional() = default;
  Optional(base::nullopt_t) : Optional() {}

  Optional(const Optional& other) {
    if (!other.is_null_)
      Init(other.value());
  }

  Optional(Optional&& other) {
    if (!other.is_null_)
      Init(std::move(other.value()));
  }

  Optional(const T& value) { Init(value); }

  Optional(T&& value) { Init(std::move(value)); }

  template <class... Args>
  explicit Optional(base::in_place_t, Args&&... args) {
    emplace(std::forward<Args>(args)...);
  }

  ~Optional() {
    // TODO(mlamouri): use is_trivially_destructible<T>::value when possible.
    FreeIfNeeded();
  }

  Optional& operator=(base::nullopt_t) {
    FreeIfNeeded();
    return *this;
  }

  Optional& operator=(const Optional& other) {
    if (other.is_null_) {
      FreeIfNeeded();
      return *this;
    }

    InitOrAssign(other.value());
    return *this;
  }

  Optional& operator=(Optional&& other) {
    if (other.is_null_) {
      FreeIfNeeded();
      return *this;
    }

    InitOrAssign(std::move(other.value()));
    return *this;
  }

  template <class U>
  typename std::enable_if<std::is_same<std::decay<U>, T>::value,
                          Optional&>::type
  operator=(U&& value) {
    InitOrAssign(std::forward<U>(value));
    return *this;
  }

  // TODO(mlamouri): can't use 'constexpr' with DCHECK.
  const T* operator->() const {
    DCHECK(!is_null_);
    return &value();
  }

  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T* operator->() {
    DCHECK(!is_null_);
    return &value();
  }

  constexpr const T& operator*() const& { return value(); }

  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T& operator*() & { return value(); }

  constexpr const T&& operator*() const&& { return std::move(value()); }

  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T&& operator*() && { return std::move(value()); }

  constexpr explicit operator bool() const { return !is_null_; }

  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T& value() & {
    DCHECK(!is_null_);
    return *buffer_.template data_as<T>();
  }

  // TODO(mlamouri): can't use 'constexpr' with DCHECK.
  const T& value() const& {
    DCHECK(!is_null_);
    return *buffer_.template data_as<T>();
  }

  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T&& value() && {
    DCHECK(!is_null_);
    return std::move(*buffer_.template data_as<T>());
  }

  // TODO(mlamouri): can't use 'constexpr' with DCHECK.
  const T&& value() const&& {
    DCHECK(!is_null_);
    return std::move(*buffer_.template data_as<T>());
  }

  template <class U>
  constexpr T value_or(U&& default_value) const& {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_copy_constructible<T>::value,
    //               "T must be copy constructible");
    static_assert(std::is_convertible<U, T>::value,
                  "U must be convertible to T");
    return is_null_ ? static_cast<T>(std::forward<U>(default_value)) : value();
  }

  template <class U>
  T value_or(U&& default_value) && {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_move_constructible<T>::value,
    //               "T must be move constructible");
    static_assert(std::is_convertible<U, T>::value,
                  "U must be convertible to T");
    return is_null_ ? static_cast<T>(std::forward<U>(default_value))
                    : std::move(value());
  }

  void swap(Optional& other) {
    if (is_null_ && other.is_null_)
      return;

    if (is_null_ != other.is_null_) {
      if (is_null_) {
        Init(std::move(*other.buffer_.template data_as<T>()));
        other.FreeIfNeeded();
      } else {
        other.Init(std::move(*buffer_.template data_as<T>()));
        FreeIfNeeded();
      }
      return;
    }

    DCHECK(!is_null_ && !other.is_null_);
    using std::swap;
    swap(**this, *other);
  }

  template <class... Args>
  void emplace(Args&&... args) {
    FreeIfNeeded();
    Init(std::forward<Args>(args)...);
  }

 private:
  void Init(const T& value) {
    DCHECK(is_null_);
    new (buffer_.template data_as<T>()) T(value);
    is_null_ = false;
  }

  void Init(T&& value) {
    DCHECK(is_null_);
    new (buffer_.template data_as<T>()) T(std::move(value));
    is_null_ = false;
  }

  template <class... Args>
  void Init(Args&&... args) {
    DCHECK(is_null_);
    new (buffer_.template data_as<T>()) T(std::forward<Args>(args)...);
    is_null_ = false;
  }

  void InitOrAssign(const T& value) {
    if (is_null_)
      Init(value);
    else
      *buffer_.template data_as<T>() = value;
  }

  void InitOrAssign(T&& value) {
    if (is_null_)
      Init(std::move(value));
    else
      *buffer_.template data_as<T>() = std::move(value);
  }

  void FreeIfNeeded() {
    if (is_null_)
      return;
    buffer_.template data_as<T>()->~T();
    is_null_ = true;
  }

  bool is_null_ = true;
  base::AlignedMemory<sizeof(T), ALIGNOF(T)> buffer_;
};

template <class T>
constexpr bool operator==(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !!lhs != !!rhs ? false : lhs == nullopt || (*lhs == *rhs);
}

template <class T>
constexpr bool operator!=(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !(lhs == rhs);
}

template <class T>
constexpr bool operator<(const Optional<T>& lhs, const Optional<T>& rhs) {
  return rhs == nullopt ? false : (lhs == nullopt ? true : *lhs < *rhs);
}

template <class T>
constexpr bool operator<=(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !(rhs < lhs);
}

template <class T>
constexpr bool operator>(const Optional<T>& lhs, const Optional<T>& rhs) {
  return rhs < lhs;
}

template <class T>
constexpr bool operator>=(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !(lhs < rhs);
}

template <class T>
constexpr bool operator==(const Optional<T>& opt, base::nullopt_t) {
  return !opt;
}

template <class T>
constexpr bool operator==(base::nullopt_t, const Optional<T>& opt) {
  return !opt;
}

template <class T>
constexpr bool operator!=(const Optional<T>& opt, base::nullopt_t) {
  return !!opt;
}

template <class T>
constexpr bool operator!=(base::nullopt_t, const Optional<T>& opt) {
  return !!opt;
}

template <class T>
constexpr bool operator<(const Optional<T>& opt, base::nullopt_t) {
  return false;
}

template <class T>
constexpr bool operator<(base::nullopt_t, const Optional<T>& opt) {
  return !!opt;
}

template <class T>
constexpr bool operator<=(const Optional<T>& opt, base::nullopt_t) {
  return !opt;
}

template <class T>
constexpr bool operator<=(base::nullopt_t, const Optional<T>& opt) {
  return true;
}

template <class T>
constexpr bool operator>(const Optional<T>& opt, base::nullopt_t) {
  return !!opt;
}

template <class T>
constexpr bool operator>(base::nullopt_t, const Optional<T>& opt) {
  return false;
}

template <class T>
constexpr bool operator>=(const Optional<T>& opt, base::nullopt_t) {
  return true;
}

template <class T>
constexpr bool operator>=(base::nullopt_t, const Optional<T>& opt) {
  return !opt;
}

template <class T>
constexpr bool operator==(const Optional<T>& opt, const T& value) {
  return opt != nullopt ? *opt == value : false;
}

template <class T>
constexpr bool operator==(const T& value, const Optional<T>& opt) {
  return opt == value;
}

template <class T>
constexpr bool operator!=(const Optional<T>& opt, const T& value) {
  return !(opt == value);
}

template <class T>
constexpr bool operator!=(const T& value, const Optional<T>& opt) {
  return !(opt == value);
}

template <class T>
constexpr bool operator<(const Optional<T>& opt, const T& value) {
  return opt != nullopt ? *opt < value : true;
}

template <class T>
constexpr bool operator<(const T& value, const Optional<T>& opt) {
  return opt != nullopt ? value < *opt : false;
}

template <class T>
constexpr bool operator<=(const Optional<T>& opt, const T& value) {
  return !(opt > value);
}

template <class T>
constexpr bool operator<=(const T& value, const Optional<T>& opt) {
  return !(value > opt);
}

template <class T>
constexpr bool operator>(const Optional<T>& opt, const T& value) {
  return value < opt;
}

template <class T>
constexpr bool operator>(const T& value, const Optional<T>& opt) {
  return opt < value;
}

template <class T>
constexpr bool operator>=(const Optional<T>& opt, const T& value) {
  return !(opt < value);
}

template <class T>
constexpr bool operator>=(const T& value, const Optional<T>& opt) {
  return !(value < opt);
}

template <class T>
constexpr Optional<typename std::decay<T>::type> make_optional(T&& value) {
  return Optional<typename std::decay<T>::type>(std::forward<T>(value));
}

template <class T>
void swap(Optional<T>& lhs, Optional<T>& rhs) {
  lhs.swap(rhs);
}

}  // namespace base

namespace std {

template <class T>
struct hash<base::Optional<T>> {
  size_t operator()(const base::Optional<T>& opt) const {
    return opt == base::nullopt ? 0 : std::hash<T>()(*opt);
  }
};

}  // namespace std

#endif  // BASE_OPTIONAL_H_
