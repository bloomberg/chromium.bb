// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#include <type_traits>
#include <utility>

#include "base/logging.h"

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

namespace internal {

template <typename T, bool = std::is_trivially_destructible<T>::value>
struct OptionalStorageBase {
  // Initializing |empty_| here instead of using default member initializing
  // to avoid errors in g++ 4.8.
  constexpr OptionalStorageBase() : empty_('\0') {}

  template <class... Args>
  constexpr explicit OptionalStorageBase(in_place_t, Args&&... args)
      : is_null_(false), value_(std::forward<Args>(args)...) {}

  // When T is not trivially destructible we must call its
  // destructor before deallocating its memory.
  ~OptionalStorageBase() {
    if (!is_null_)
      value_.~T();
  }

  template <class... Args>
  void Init(Args&&... args) {
    DCHECK(is_null_);
    ::new (&value_) T(std::forward<Args>(args)...);
    is_null_ = false;
  }

  bool is_null_ = true;
  union {
    // |empty_| exists so that the union will always be initialized, even when
    // it doesn't contain a value. Union members must be initialized for the
    // constructor to be 'constexpr'.
    char empty_;
    T value_;
  };
};

template <typename T>
struct OptionalStorageBase<T, true /* trivially destructible */> {
  // Initializing |empty_| here instead of using default member initializing
  // to avoid errors in g++ 4.8.
  constexpr OptionalStorageBase() : empty_('\0') {}

  template <class... Args>
  constexpr explicit OptionalStorageBase(in_place_t, Args&&... args)
      : is_null_(false), value_(std::forward<Args>(args)...) {}

  // When T is trivially destructible (i.e. its destructor does nothing) there
  // is no need to call it. Explicitly defaulting the destructor means it's not
  // user-provided. Those two together make this destructor trivial.
  ~OptionalStorageBase() = default;

  template <class... Args>
  void Init(Args&&... args) {
    DCHECK(is_null_);
    ::new (&value_) T(std::forward<Args>(args)...);
    is_null_ = false;
  }

  bool is_null_ = true;
  union {
    // |empty_| exists so that the union will always be initialized, even when
    // it doesn't contain a value. Union members must be initialized for the
    // constructor to be 'constexpr'.
    char empty_;
    T value_;
  };
};

// Implement conditional constexpr copy and move constructors. These are
// constexpr if is_trivially_{copy,move}_constructible<T>::value is true
// respectively. If each is true, the corresponding constructor is defined as
// "= default;", which generates a constexpr constructor (In this case,
// the condition of constexpr-ness is satisfied because the base class also has
// compiler generated constexpr {copy,move} constructors). Note that
// placement-new is prohibited in constexpr.
template <typename T,
          bool = std::is_trivially_copy_constructible<T>::value,
          bool = std::is_trivially_move_constructible<T>::value>
struct OptionalStorage : OptionalStorageBase<T> {
  // This is no trivially {copy,move} constructible case. Other cases are
  // defined below as specializations.

  // Accessing the members of template base class requires explicit
  // declaration.
  using OptionalStorageBase<T>::is_null_;
  using OptionalStorageBase<T>::value_;
  using OptionalStorageBase<T>::Init;

  // Inherit constructors (specifically, the in_place constructor).
  using OptionalStorageBase<T>::OptionalStorageBase;

  // User defined constructor deletes the default constructor.
  // Define it explicitly.
  OptionalStorage() = default;

  OptionalStorage(const OptionalStorage& other) {
    if (!other.is_null_)
      Init(other.value_);
  }

  OptionalStorage(OptionalStorage&& other) {
    if (!other.is_null_)
      Init(std::move(other.value_));
  }
};

template <typename T>
struct OptionalStorage<T,
                       true /* trivially copy constructible */,
                       false /* trivially move constructible */>
    : OptionalStorageBase<T> {
  using OptionalStorageBase<T>::is_null_;
  using OptionalStorageBase<T>::value_;
  using OptionalStorageBase<T>::Init;
  using OptionalStorageBase<T>::OptionalStorageBase;

  OptionalStorage() = default;
  OptionalStorage(const OptionalStorage& other) = default;

  OptionalStorage(OptionalStorage&& other) {
    if (!other.is_null_)
      Init(std::move(other.value_));
  }
};

template <typename T>
struct OptionalStorage<T,
                       false /* trivially copy constructible */,
                       true /* trivially move constructible */>
    : OptionalStorageBase<T> {
  using OptionalStorageBase<T>::is_null_;
  using OptionalStorageBase<T>::value_;
  using OptionalStorageBase<T>::Init;
  using OptionalStorageBase<T>::OptionalStorageBase;

  OptionalStorage() = default;
  OptionalStorage(OptionalStorage&& other) = default;

  OptionalStorage(const OptionalStorage& other) {
    if (!other.is_null_)
      Init(other.value_);
  }
};

template <typename T>
struct OptionalStorage<T,
                       true /* trivially copy constructible */,
                       true /* trivially move constructible */>
    : OptionalStorageBase<T> {
  // If both trivially {copy,move} constructible are true, it is not necessary
  // to use user-defined constructors. So, just inheriting constructors
  // from the base class works.
  using OptionalStorageBase<T>::OptionalStorageBase;
};

// Base class to support conditionally usable copy-/move- constructors
// and assign operators.
template <typename T>
class OptionalBase {
  // This class provides implementation rather than public API, so everything
  // should be hidden. Often we use composition, but we cannot in this case
  // because of C++ language restriction.
 protected:
  constexpr OptionalBase() = default;
  constexpr OptionalBase(const OptionalBase& other) = default;
  constexpr OptionalBase(OptionalBase&& other) = default;

  template <class... Args>
  constexpr explicit OptionalBase(in_place_t, Args&&... args)
      : storage_(in_place, std::forward<Args>(args)...) {}

  ~OptionalBase() = default;

  OptionalBase& operator=(const OptionalBase& other) {
    if (other.storage_.is_null_) {
      FreeIfNeeded();
      return *this;
    }

    InitOrAssign(other.storage_.value_);
    return *this;
  }

  OptionalBase& operator=(OptionalBase&& other) {
    if (other.storage_.is_null_) {
      FreeIfNeeded();
      return *this;
    }

    InitOrAssign(std::move(other.storage_.value_));
    return *this;
  }

  void InitOrAssign(const T& value) {
    if (storage_.is_null_)
      storage_.Init(value);
    else
      storage_.value_ = value;
  }

  void InitOrAssign(T&& value) {
    if (storage_.is_null_)
      storage_.Init(std::move(value));
    else
      storage_.value_ = std::move(value);
  }

  void FreeIfNeeded() {
    if (storage_.is_null_)
      return;
    storage_.value_.~T();
    storage_.is_null_ = true;
  }

  OptionalStorage<T> storage_;
};

}  // namespace internal

// base::Optional is a Chromium version of the C++17 optional class:
// std::optional documentation:
// http://en.cppreference.com/w/cpp/utility/optional
// Chromium documentation:
// https://chromium.googlesource.com/chromium/src/+/master/docs/optional.md
//
// These are the differences between the specification and the implementation:
// - Constructors do not use 'constexpr' as it is a C++14 extension.
// - 'constexpr' might be missing in some places for reasons specified locally.
// - No exceptions are thrown, because they are banned from Chromium.
// - All the non-members are in the 'base' namespace instead of 'std'.
template <typename T>
class Optional : public internal::OptionalBase<T> {
 public:
  using value_type = T;

  // Defer default/copy/move constructor implementation to OptionalBase.
  // TODO(hidehiko): Implement conditional enabling.
  constexpr Optional() = default;
  constexpr Optional(const Optional& other) = default;
  constexpr Optional(Optional&& other) = default;

  constexpr Optional(nullopt_t) {}

  constexpr Optional(const T& value)
      : internal::OptionalBase<T>(in_place, value) {}

  constexpr Optional(T&& value)
      : internal::OptionalBase<T>(in_place, std::move(value)) {}

  template <class... Args>
  constexpr explicit Optional(in_place_t, Args&&... args)
      : internal::OptionalBase<T>(in_place, std::forward<Args>(args)...) {}

  template <
      class U,
      class... Args,
      class = std::enable_if_t<std::is_constructible<value_type,
                                                     std::initializer_list<U>&,
                                                     Args...>::value>>
  constexpr explicit Optional(in_place_t,
                              std::initializer_list<U> il,
                              Args&&... args)
      : internal::OptionalBase<T>(in_place, il, std::forward<Args>(args)...) {}

  ~Optional() = default;

  // Defer copy-/move- assign operator implementation to OptionalBase.
  // TOOD(hidehiko): Implement conditional enabling.
  Optional& operator=(const Optional& other) = default;
  Optional& operator=(Optional&& other) = default;

  Optional& operator=(nullopt_t) {
    FreeIfNeeded();
    return *this;
  }

  template <class U>
  typename std::enable_if<std::is_same<std::decay_t<U>, T>::value,
                          Optional&>::type
  operator=(U&& value) {
    InitOrAssign(std::forward<U>(value));
    return *this;
  }

  constexpr const T* operator->() const {
    DCHECK(!storage_.is_null_);
    return &value();
  }

  constexpr T* operator->() {
    DCHECK(!storage_.is_null_);
    return &value();
  }

  constexpr const T& operator*() const& { return value(); }

  constexpr T& operator*() & { return value(); }

  constexpr const T&& operator*() const&& { return std::move(value()); }

  constexpr T&& operator*() && { return std::move(value()); }

  constexpr explicit operator bool() const { return !storage_.is_null_; }

  constexpr bool has_value() const { return !storage_.is_null_; }

  constexpr T& value() & {
    DCHECK(!storage_.is_null_);
    return storage_.value_;
  }

  constexpr const T& value() const & {
    DCHECK(!storage_.is_null_);
    return storage_.value_;
  }

  constexpr T&& value() && {
    DCHECK(!storage_.is_null_);
    return std::move(storage_.value_);
  }

  constexpr const T&& value() const && {
    DCHECK(!storage_.is_null_);
    return std::move(storage_.value_);
  }

  template <class U>
  constexpr T value_or(U&& default_value) const& {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_copy_constructible<T>::value,
    //               "T must be copy constructible");
    static_assert(std::is_convertible<U, T>::value,
                  "U must be convertible to T");
    return storage_.is_null_ ? static_cast<T>(std::forward<U>(default_value))
                             : value();
  }

  template <class U>
  T value_or(U&& default_value) && {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_move_constructible<T>::value,
    //               "T must be move constructible");
    static_assert(std::is_convertible<U, T>::value,
                  "U must be convertible to T");
    return storage_.is_null_ ? static_cast<T>(std::forward<U>(default_value))
                             : std::move(value());
  }

  void swap(Optional& other) {
    if (storage_.is_null_ && other.storage_.is_null_)
      return;

    if (storage_.is_null_ != other.storage_.is_null_) {
      if (storage_.is_null_) {
        storage_.Init(std::move(other.storage_.value_));
        other.FreeIfNeeded();
      } else {
        other.storage_.Init(std::move(storage_.value_));
        FreeIfNeeded();
      }
      return;
    }

    DCHECK(!storage_.is_null_ && !other.storage_.is_null_);
    using std::swap;
    swap(**this, *other);
  }

  void reset() {
    FreeIfNeeded();
  }

  template <class... Args>
  void emplace(Args&&... args) {
    FreeIfNeeded();
    storage_.Init(std::forward<Args>(args)...);
  }

  template <
      class U,
      class... Args,
      class = std::enable_if_t<std::is_constructible<value_type,
                                                     std::initializer_list<U>&,
                                                     Args...>::value>>
  T& emplace(std::initializer_list<U> il, Args&&... args) {
    FreeIfNeeded();
    storage_.Init(il, std::forward<Args>(args)...);
    return storage_.value_;
  }

 private:
  // Accessing template base class's protected member needs explicit
  // declaration to do so.
  using internal::OptionalBase<T>::FreeIfNeeded;
  using internal::OptionalBase<T>::InitOrAssign;
  using internal::OptionalBase<T>::storage_;
};

// Here after defines comparation operators. The definition follows
// http://en.cppreference.com/w/cpp/utility/optional/operator_cmp
// while bool() casting is replaced by has_value() to meet the chromium
// style guide.
template <class T, class U>
constexpr bool operator==(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (lhs.has_value() != rhs.has_value())
    return false;
  if (!lhs.has_value())
    return true;
  return *lhs == *rhs;
}

template <class T, class U>
constexpr bool operator!=(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (lhs.has_value() != rhs.has_value())
    return true;
  if (!lhs.has_value())
    return false;
  return *lhs != *rhs;
}

template <class T, class U>
constexpr bool operator<(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (!rhs.has_value())
    return false;
  if (!lhs.has_value())
    return true;
  return *lhs < *rhs;
}

template <class T, class U>
constexpr bool operator<=(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (!lhs.has_value())
    return true;
  if (!rhs.has_value())
    return false;
  return *lhs <= *rhs;
}

template <class T, class U>
constexpr bool operator>(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (!lhs.has_value())
    return false;
  if (!rhs.has_value())
    return true;
  return *lhs > *rhs;
}

template <class T, class U>
constexpr bool operator>=(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (!rhs.has_value())
    return true;
  if (!lhs.has_value())
    return false;
  return *lhs >= *rhs;
}

template <class T>
constexpr bool operator==(const Optional<T>& opt, nullopt_t) {
  return !opt;
}

template <class T>
constexpr bool operator==(nullopt_t, const Optional<T>& opt) {
  return !opt;
}

template <class T>
constexpr bool operator!=(const Optional<T>& opt, nullopt_t) {
  return opt.has_value();
}

template <class T>
constexpr bool operator!=(nullopt_t, const Optional<T>& opt) {
  return opt.has_value();
}

template <class T>
constexpr bool operator<(const Optional<T>& opt, nullopt_t) {
  return false;
}

template <class T>
constexpr bool operator<(nullopt_t, const Optional<T>& opt) {
  return opt.has_value();
}

template <class T>
constexpr bool operator<=(const Optional<T>& opt, nullopt_t) {
  return !opt;
}

template <class T>
constexpr bool operator<=(nullopt_t, const Optional<T>& opt) {
  return true;
}

template <class T>
constexpr bool operator>(const Optional<T>& opt, nullopt_t) {
  return opt.has_value();
}

template <class T>
constexpr bool operator>(nullopt_t, const Optional<T>& opt) {
  return false;
}

template <class T>
constexpr bool operator>=(const Optional<T>& opt, nullopt_t) {
  return true;
}

template <class T>
constexpr bool operator>=(nullopt_t, const Optional<T>& opt) {
  return !opt;
}

template <class T, class U>
constexpr bool operator==(const Optional<T>& opt, const U& value) {
  return opt.has_value() ? *opt == value : false;
}

template <class T, class U>
constexpr bool operator==(const U& value, const Optional<T>& opt) {
  return opt.has_value() ? value == *opt : false;
}

template <class T, class U>
constexpr bool operator!=(const Optional<T>& opt, const U& value) {
  return opt.has_value() ? *opt != value : true;
}

template <class T, class U>
constexpr bool operator!=(const U& value, const Optional<T>& opt) {
  return opt.has_value() ? value != *opt : true;
}

template <class T, class U>
constexpr bool operator<(const Optional<T>& opt, const U& value) {
  return opt.has_value() ? *opt < value : true;
}

template <class T, class U>
constexpr bool operator<(const U& value, const Optional<T>& opt) {
  return opt.has_value() ? value < *opt : false;
}

template <class T, class U>
constexpr bool operator<=(const Optional<T>& opt, const U& value) {
  return opt.has_value() ? *opt <= value : true;
}

template <class T, class U>
constexpr bool operator<=(const U& value, const Optional<T>& opt) {
  return opt.has_value() ? value <= *opt : false;
}

template <class T, class U>
constexpr bool operator>(const Optional<T>& opt, const U& value) {
  return opt.has_value() ? *opt > value : false;
}

template <class T, class U>
constexpr bool operator>(const U& value, const Optional<T>& opt) {
  return opt.has_value() ? value > *opt : true;
}

template <class T, class U>
constexpr bool operator>=(const Optional<T>& opt, const U& value) {
  return opt.has_value() ? *opt >= value : false;
}

template <class T, class U>
constexpr bool operator>=(const U& value, const Optional<T>& opt) {
  return opt.has_value() ? value >= *opt : true;
}

template <class T>
constexpr Optional<typename std::decay<T>::type> make_optional(T&& value) {
  return Optional<typename std::decay<T>::type>(std::forward<T>(value));
}

template <class T, class U, class... Args>
constexpr Optional<T> make_optional(std::initializer_list<U> il,
                                    Args&&... args) {
  return Optional<T>(in_place, il, std::forward<Args>(args)...);
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
