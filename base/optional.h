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

// Forward declaration, which is refered by following helpers.
template <typename T>
class Optional;

namespace internal {

template <typename T, bool = std::is_trivially_destructible<T>::value>
struct OptionalStorageBase {
  // Initializing |empty_| here instead of using default member initializing
  // to avoid errors in g++ 4.8.
  constexpr OptionalStorageBase() : empty_('\0') {}

  template <class... Args>
  constexpr explicit OptionalStorageBase(in_place_t, Args&&... args)
      : is_populated_(true), value_(std::forward<Args>(args)...) {}

  // When T is not trivially destructible we must call its
  // destructor before deallocating its memory.
  // Note that this hides the (implicitly declared) move constructor, which
  // would be used for constexpr move constructor in OptionalStorage<T>.
  // It is needed iff T is trivially move constructible. However, the current
  // is_trivially_{copy,move}_constructible implementation requires
  // is_trivially_destructible (which looks a bug, cf:
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=51452 and
  // http://cplusplus.github.io/LWG/lwg-active.html#2116), so it is not
  // necessary for this case at the moment. Please see also the destructor
  // comment in "is_trivially_destructible = true" specialization below.
  ~OptionalStorageBase() {
    if (is_populated_)
      value_.~T();
  }

  template <class... Args>
  void Init(Args&&... args) {
    DCHECK(!is_populated_);
    ::new (&value_) T(std::forward<Args>(args)...);
    is_populated_ = true;
  }

  bool is_populated_ = false;
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
      : is_populated_(true), value_(std::forward<Args>(args)...) {}

  // When T is trivially destructible (i.e. its destructor does nothing) there
  // is no need to call it. Implicitly defined destructor is trivial, because
  // both members (bool and union containing only variants which are trivially
  // destructible) are trivially destructible.
  // Explicitly-defaulted destructor is also trivial, but do not use it here,
  // because it hides the implicit move constructor. It is needed to implement
  // constexpr move constructor in OptionalStorage iff T is trivially move
  // constructible. Note that, if T is trivially move constructible, the move
  // constructor of OptionalStorageBase<T> is also implicitly defined and it is
  // trivially move constructor. If T is not trivially move constructible,
  // "not declaring move constructor without destructor declaration" here means
  // "delete move constructor", which works because any move constructor of
  // OptionalStorage will not refer to it in that case.

  template <class... Args>
  void Init(Args&&... args) {
    DCHECK(!is_populated_);
    ::new (&value_) T(std::forward<Args>(args)...);
    is_populated_ = true;
  }

  bool is_populated_ = false;
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
  using OptionalStorageBase<T>::is_populated_;
  using OptionalStorageBase<T>::value_;
  using OptionalStorageBase<T>::Init;

  // Inherit constructors (specifically, the in_place constructor).
  using OptionalStorageBase<T>::OptionalStorageBase;

  // User defined constructor deletes the default constructor.
  // Define it explicitly.
  OptionalStorage() = default;

  OptionalStorage(const OptionalStorage& other) {
    if (other.is_populated_)
      Init(other.value_);
  }

  OptionalStorage(OptionalStorage&& other) {
    if (other.is_populated_)
      Init(std::move(other.value_));
  }
};

template <typename T>
struct OptionalStorage<T,
                       true /* trivially copy constructible */,
                       false /* trivially move constructible */>
    : OptionalStorageBase<T> {
  using OptionalStorageBase<T>::is_populated_;
  using OptionalStorageBase<T>::value_;
  using OptionalStorageBase<T>::Init;
  using OptionalStorageBase<T>::OptionalStorageBase;

  OptionalStorage() = default;
  OptionalStorage(const OptionalStorage& other) = default;

  OptionalStorage(OptionalStorage&& other) {
    if (other.is_populated_)
      Init(std::move(other.value_));
  }
};

template <typename T>
struct OptionalStorage<T,
                       false /* trivially copy constructible */,
                       true /* trivially move constructible */>
    : OptionalStorageBase<T> {
  using OptionalStorageBase<T>::is_populated_;
  using OptionalStorageBase<T>::value_;
  using OptionalStorageBase<T>::Init;
  using OptionalStorageBase<T>::OptionalStorageBase;

  OptionalStorage() = default;
  OptionalStorage(OptionalStorage&& other) = default;

  OptionalStorage(const OptionalStorage& other) {
    if (other.is_populated_)
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

  // Implementation of converting constructors.
  template <typename U>
  explicit OptionalBase(const OptionalBase<U>& other) {
    if (other.storage_.is_populated_)
      storage_.Init(other.storage_.value_);
  }

  template <typename U>
  explicit OptionalBase(OptionalBase<U>&& other) {
    if (other.storage_.is_populated_)
      storage_.Init(std::move(other.storage_.value_));
  }

  ~OptionalBase() = default;

  OptionalBase& operator=(const OptionalBase& other) {
    if (!other.storage_.is_populated_) {
      FreeIfNeeded();
      return *this;
    }

    InitOrAssign(other.storage_.value_);
    return *this;
  }

  OptionalBase& operator=(OptionalBase&& other) {
    if (!other.storage_.is_populated_) {
      FreeIfNeeded();
      return *this;
    }

    InitOrAssign(std::move(other.storage_.value_));
    return *this;
  }

  void InitOrAssign(const T& value) {
    if (!storage_.is_populated_)
      storage_.Init(value);
    else
      storage_.value_ = value;
  }

  void InitOrAssign(T&& value) {
    if (!storage_.is_populated_)
      storage_.Init(std::move(value));
    else
      storage_.value_ = std::move(value);
  }

  void FreeIfNeeded() {
    if (!storage_.is_populated_)
      return;
    storage_.value_.~T();
    storage_.is_populated_ = false;
  }

  // For implementing conversion, allow access to other typed OptionalBase
  // class.
  template <typename U>
  friend class OptionalBase;

  OptionalStorage<T> storage_;
};

// The following {Copy,Move}{Constructible,Assignable} structs are helpers to
// implement constructor/assign-operator overloading. Specifically, if T is
// is not movable but copyable, Optional<T>'s move constructor should not
// participate in overload resolution. This inheritance trick implements that.
template <bool is_copy_constructible>
struct CopyConstructible {};

template <>
struct CopyConstructible<false> {
  constexpr CopyConstructible() = default;
  constexpr CopyConstructible(const CopyConstructible&) = delete;
  constexpr CopyConstructible(CopyConstructible&&) = default;
  CopyConstructible& operator=(const CopyConstructible&) = default;
  CopyConstructible& operator=(CopyConstructible&&) = default;
};

template <bool is_move_constructible>
struct MoveConstructible {};

template <>
struct MoveConstructible<false> {
  constexpr MoveConstructible() = default;
  constexpr MoveConstructible(const MoveConstructible&) = default;
  constexpr MoveConstructible(MoveConstructible&&) = delete;
  MoveConstructible& operator=(const MoveConstructible&) = default;
  MoveConstructible& operator=(MoveConstructible&&) = default;
};

template <bool is_copy_assignable>
struct CopyAssignable {};

template <>
struct CopyAssignable<false> {
  constexpr CopyAssignable() = default;
  constexpr CopyAssignable(const CopyAssignable&) = default;
  constexpr CopyAssignable(CopyAssignable&&) = default;
  CopyAssignable& operator=(const CopyAssignable&) = delete;
  CopyAssignable& operator=(CopyAssignable&&) = default;
};

template <bool is_move_assignable>
struct MoveAssignable {};

template <>
struct MoveAssignable<false> {
  constexpr MoveAssignable() = default;
  constexpr MoveAssignable(const MoveAssignable&) = default;
  constexpr MoveAssignable(MoveAssignable&&) = default;
  MoveAssignable& operator=(const MoveAssignable&) = default;
  MoveAssignable& operator=(MoveAssignable&&) = delete;
};

// Helper to conditionally enable converting constructors.
template <typename T, typename U>
struct IsConvertibleFromOptional
    : std::integral_constant<
          bool,
          std::is_constructible<T, Optional<U>&>::value ||
              std::is_constructible<T, const Optional<U>&>::value ||
              std::is_constructible<T, Optional<U>&&>::value ||
              std::is_constructible<T, const Optional<U>&&>::value ||
              std::is_convertible<Optional<U>&, T>::value ||
              std::is_convertible<const Optional<U>&, T>::value ||
              std::is_convertible<Optional<U>&&, T>::value ||
              std::is_convertible<const Optional<U>&&, T>::value> {};

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
class Optional
    : public internal::OptionalBase<T>,
      public internal::CopyConstructible<std::is_copy_constructible<T>::value>,
      public internal::MoveConstructible<std::is_move_constructible<T>::value>,
      public internal::CopyAssignable<std::is_copy_constructible<T>::value &&
                                      std::is_copy_assignable<T>::value>,
      public internal::MoveAssignable<std::is_move_constructible<T>::value &&
                                      std::is_move_assignable<T>::value> {
 public:
  using value_type = T;

  // Defer default/copy/move constructor implementation to OptionalBase.
  constexpr Optional() = default;
  constexpr Optional(const Optional& other) = default;
  constexpr Optional(Optional&& other) = default;

  constexpr Optional(nullopt_t) {}  // NOLINT(runtime/explicit)

  // Converting copy constructor. "explicit" only if
  // std::is_convertible<const U&, T>::value is false. It is implemented by
  // declaring two almost same constructors, but that condition in enable_if_t
  // is different, so that either one is chosen, thanks to SFINAE.
  template <
      typename U,
      std::enable_if_t<std::is_constructible<T, const U&>::value &&
                           !internal::IsConvertibleFromOptional<T, U>::value &&
                           std::is_convertible<const U&, T>::value,
                       bool> = false>
  Optional(const Optional<U>& other) : internal::OptionalBase<T>(other) {}

  template <
      typename U,
      std::enable_if_t<std::is_constructible<T, const U&>::value &&
                           !internal::IsConvertibleFromOptional<T, U>::value &&
                           !std::is_convertible<const U&, T>::value,
                       bool> = false>
  explicit Optional(const Optional<U>& other)
      : internal::OptionalBase<T>(other) {}

  // Converting move constructor. Similar to converting copy constructor,
  // declaring two (explicit and non-explicit) constructors.
  template <
      typename U,
      std::enable_if_t<std::is_constructible<T, U&&>::value &&
                           !internal::IsConvertibleFromOptional<T, U>::value &&
                           std::is_convertible<U&&, T>::value,
                       bool> = false>
  Optional(Optional<U>&& other) : internal::OptionalBase<T>(std::move(other)) {}

  template <
      typename U,
      std::enable_if_t<std::is_constructible<T, U&&>::value &&
                           !internal::IsConvertibleFromOptional<T, U>::value &&
                           !std::is_convertible<U&&, T>::value,
                       bool> = false>
  explicit Optional(Optional<U>&& other)
      : internal::OptionalBase<T>(std::move(other)) {}

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
    DCHECK(storage_.is_populated_);
    return &value();
  }

  constexpr T* operator->() {
    DCHECK(storage_.is_populated_);
    return &value();
  }

  constexpr const T& operator*() const& { return value(); }

  constexpr T& operator*() & { return value(); }

  constexpr const T&& operator*() const&& { return std::move(value()); }

  constexpr T&& operator*() && { return std::move(value()); }

  constexpr explicit operator bool() const { return storage_.is_populated_; }

  constexpr bool has_value() const { return storage_.is_populated_; }

  constexpr T& value() & {
    DCHECK(storage_.is_populated_);
    return storage_.value_;
  }

  constexpr const T& value() const & {
    DCHECK(storage_.is_populated_);
    return storage_.value_;
  }

  constexpr T&& value() && {
    DCHECK(storage_.is_populated_);
    return std::move(storage_.value_);
  }

  constexpr const T&& value() const && {
    DCHECK(storage_.is_populated_);
    return std::move(storage_.value_);
  }

  template <class U>
  constexpr T value_or(U&& default_value) const& {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_copy_constructible<T>::value,
    //               "T must be copy constructible");
    static_assert(std::is_convertible<U, T>::value,
                  "U must be convertible to T");
    return storage_.is_populated_
               ? value()
               : static_cast<T>(std::forward<U>(default_value));
  }

  template <class U>
  T value_or(U&& default_value) && {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_move_constructible<T>::value,
    //               "T must be move constructible");
    static_assert(std::is_convertible<U, T>::value,
                  "U must be convertible to T");
    return storage_.is_populated_
               ? std::move(value())
               : static_cast<T>(std::forward<U>(default_value));
  }

  void swap(Optional& other) {
    if (!storage_.is_populated_ && !other.storage_.is_populated_)
      return;

    if (storage_.is_populated_ != other.storage_.is_populated_) {
      if (storage_.is_populated_) {
        other.storage_.Init(std::move(storage_.value_));
        FreeIfNeeded();
      } else {
        storage_.Init(std::move(other.storage_.value_));
        other.FreeIfNeeded();
      }
      return;
    }

    DCHECK(storage_.is_populated_ && other.storage_.is_populated_);
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
