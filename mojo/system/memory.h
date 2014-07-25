// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MEMORY_H_
#define MOJO_SYSTEM_MEMORY_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>  // For |memcpy()|.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

namespace internal {

// Removes |const| from |T| (available as |remove_const<T>::type|):
// TODO(vtl): Remove these once we have the C++11 |remove_const|.
template <typename T> struct remove_const { typedef T type; };
template <typename T> struct remove_const<const T> { typedef T type; };

// Yields |(const) char| if |T| is |(const) void|, else |T|:
template <typename T> struct VoidToChar { typedef T type; };
template <> struct VoidToChar<void> { typedef char type; };
template <> struct VoidToChar<const void> { typedef const char type; };

template <size_t size, size_t alignment>
void MOJO_SYSTEM_IMPL_EXPORT CheckUserPointerHelper(const void* pointer);

template <size_t size, size_t alignment>
void MOJO_SYSTEM_IMPL_EXPORT CheckUserPointerWithCountHelper(
    const void* pointer,
    size_t count);

// TODO(vtl): Delete all the |Verify...()| things (and replace them with
// |Check...()|.
template <size_t size, size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper(const void* pointer);

// Note: This is also used by options_validation.h.
template <size_t size, size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithCountHelper(
    const void* pointer,
    size_t count);

}  // namespace internal

// Forward declarations so that they can be friended.
template <typename Type> class UserPointerReader;
template <typename Type> class UserPointerWriter;
template <typename Type> class UserPointerReaderWriter;

// Verify (insofar as possible/necessary) that a |T| can be read from the user
// |pointer|.
template <typename T>
bool VerifyUserPointer(const T* pointer) {
  return internal::VerifyUserPointerHelper<sizeof(T), MOJO_ALIGNOF(T)>(pointer);
}

// Verify (insofar as possible/necessary) that |count| |T|s can be read from the
// user |pointer|; |count| may be zero. (This is done carefully since |count *
// sizeof(T)| may overflow a |size_t|.)
template <typename T>
bool VerifyUserPointerWithCount(const T* pointer, size_t count) {
  return internal::VerifyUserPointerWithCountHelper<sizeof(T),
                                                    MOJO_ALIGNOF(T)>(pointer,
                                                                     count);
}

// Verify that |size| bytes (which may be zero) can be read from the user
// |pointer|, and that |pointer| has the specified |alignment| (if |size| is
// nonzero).
template <size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithSize(const void* pointer,
                                                       size_t size);

// Provides a convenient way to implicitly get null |UserPointer<Type>|s.
struct NullUserPointer {};

// Represents a user pointer to a single |Type| (which must be POD), for Mojo
// primitive parameters.
//
// Use a const |Type| for in parameters, and non-const |Type|s for out and
// in-out parameters (in which case the |Put()| method is available).
template <typename Type>
class UserPointer {
 private:
  typedef typename internal::VoidToChar<Type>::type NonVoidType;

 public:
  // Instead of explicitly using these constructors, you can often use
  // |MakeUserPointer()| (or |NullUserPointer()| for null pointers). (The common
  // exception is when you have, e.g., a |char*| and want to get a
  // |UserPointer<void>|.)
  UserPointer() : pointer_(NULL) {}
  explicit UserPointer(Type* pointer) : pointer_(pointer) {}
  // Allow implicit conversion from the "null user pointer".
  UserPointer(NullUserPointer) : pointer_(NULL) {}
  ~UserPointer() {}

  // Allow assignment from the "null user pointer".
  UserPointer<Type>& operator=(NullUserPointer) {
    pointer_ = NULL;
    return *this;
  }

  // Allow conversion to a "non-const" |UserPointer|.
  operator UserPointer<const Type>() const {
    return UserPointer<const Type>(pointer_);
  }

  bool IsNull() const {
    return !pointer_;
  }

  // Gets the value (of type |Type|) pointed to by this user pointer. Use this
  // when you'd use the rvalue |*user_pointer|, but be aware that this may be
  // costly -- so if the value will be used multiple times, you should save it.
  //
  // (We want to force a copy here, so return |Type| not |const Type&|.)
  Type Get() const {
    internal::CheckUserPointerHelper<sizeof(Type),
                                     MOJO_ALIGNOF(Type)>(pointer_);
    return *pointer_;
  }

  // TODO(vtl): Add a |GetArray()| method (see |PutArray()|).

  // Puts a value (of type |Type|, or of type |char| if |Type| is |void|) to the
  // location pointed to by this user pointer. Use this when you'd use the
  // lvalue |*user_pointer|. Since this may be costly, you should avoid using
  // this (for the same user pointer) more than once.
  //
  // Note: This |Put()| method is not valid when |T| is const, e.g., |const
  // uint32_t|, but it's okay to include them so long as this template is only
  // implicitly instantiated (see 14.7.1 of the C++11 standard) and not
  // explicitly instantiated. (On implicit instantiation, only the declarations
  // need be valid, not the definitions.)
  //
  // If |Type| is |void|, we "convert" it to |char|, so that it makes sense.
  // (Otherwise, we'd need a suitable specialization to exclude |Put()|.)
  //
  // In C++11, we could do something like:
  //   template <typename _Type = Type>
  //   typename enable_if<!is_const<_Type>::value &&
  //                      !is_void<_Type>::value>::type Put(
  //       const _Type& value) { ... }
  // (which obviously be correct), but C++03 doesn't allow default function
  // template arguments.
  void Put(const NonVoidType& value) {
    internal::CheckUserPointerHelper<sizeof(NonVoidType),
                                     MOJO_ALIGNOF(NonVoidType)>(pointer_);
    *pointer_ = value;
  }

  // Puts an array (of type |Type|, or just a buffer if |Type| is |void|) and
  // size |count| (number of elements, or number of bytes if |Type| is |void|)
  // to the location pointed to by this user pointer. Use this when you'd do
  // something like |memcpy(user_pointer, source, count * sizeof(Type))|.
  //
  // Note: The same comments about the validity of |Put()| (except for the part
  // about |void|) apply here.
  void PutArray(const Type* source, size_t count) {
    internal::CheckUserPointerWithCountHelper<sizeof(NonVoidType),
                                              MOJO_ALIGNOF(NonVoidType)>(source,
                                                                         count);
    memcpy(pointer_, source, count * sizeof(NonVoidType));
  }

  // Gets a |UserPointer| at offset |i| (in |Type|s) relative to this. This
  // method is not valid if |Type| is |void| (TODO(vtl): Maybe I should make it
  // valid, with the offset in bytes?).
  UserPointer At(size_t i) const {
    return UserPointer(pointer_ + i);
  }

  // TODO(vtl): This is temporary. Get rid of this. (We should pass
  // |UserPointer<>|s along appropriately, or access data in a safe way
  // everywhere.)
  Type* GetPointerUnsafe() const {
    return pointer_;
  }

  // These provides safe (read-only/write-only/read-and-write) access to a
  // |UserPointer<Type>| (probably pointing to an array) using just an ordinary
  // pointer (obtained via |GetPointer()|).
  //
  // The memory returned by |GetPointer()| may be a copy of the original user
  // memory, but should be modified only if the user is intended to eventually
  // see the change.) If any changes are made, |Commit()| should be called to
  // guarantee that the changes are written back to user memory (it may be
  // called multiple times).
  //
  // Note: These classes are designed to allow fast, unsafe implementations (in
  // which |GetPointer()| just returns the user pointer) if desired. Thus if
  // |Commit()| is *not* called, changes may or may not be made visible to the
  // user.
  //
  // Use these classes in the following way:
  //
  //   MojoResult Core::PutFoos(UserPointer<const uint32_t> foos,
  //                            uint32_t num_foos) {
  //     UserPointer<const uint32_t>::Reader foos_reader(foos, num_foos);
  //     return PutFoosImpl(foos_reader.GetPointer(), num_foos);
  //   }
  //
  //   MojoResult Core::GetFoos(UserPointer<uint32_t> foos,
  //                            uint32_t num_foos) {
  //     UserPointer<uint32_t>::Writer foos_writer(foos, num_foos);
  //     MojoResult rv = GetFoosImpl(foos.GetPointer(), num_foos);
  //     foos_writer.Commit();
  //     return rv;
  //   }
  //
  // TODO(vtl): Possibly, since we're not really being safe, we should just not
  // copy for Release builds.
  typedef UserPointerReader<Type> Reader;
  typedef UserPointerWriter<Type> Writer;
  typedef UserPointerReaderWriter<Type> ReaderWriter;

 private:
  friend class UserPointerReader<Type>;
  friend class UserPointerWriter<Type>;
  friend class UserPointerReaderWriter<Type>;

  Type* pointer_;
  // Allow copy and assignment.
};

// Provides a convenient way to make a |UserPointer<Type>|.
template <typename Type>
inline UserPointer<Type> MakeUserPointer(Type* pointer) {
  return UserPointer<Type>(pointer);
}

// Implementation of |UserPointer<Type>::Reader|.
template <typename Type>
class UserPointerReader {
 private:
  typedef typename internal::remove_const<Type>::type TypeNoConst;

 public:
  UserPointerReader(UserPointer<const Type> user_pointer, size_t count) {
    Init(user_pointer.pointer_, count);
  }
  UserPointerReader(UserPointer<TypeNoConst> user_pointer, size_t count) {
    Init(user_pointer.pointer_, count);
  }

  const Type* GetPointer() const { return buffer_.get(); }

 private:
  void Init(const Type* user_pointer, size_t count) {
    internal::CheckUserPointerWithCountHelper<
        sizeof(Type), MOJO_ALIGNOF(Type)>(user_pointer, count);
    buffer_.reset(new TypeNoConst[count]);
    memcpy(buffer_.get(), user_pointer, count * sizeof(Type));
  }

  scoped_ptr<TypeNoConst[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(UserPointerReader);
};

// Implementation of |UserPointer<Type>::Writer|.
template <typename Type>
class UserPointerWriter {
 public:
  UserPointerWriter(UserPointer<Type> user_pointer, size_t count)
      : user_pointer_(user_pointer),
        count_(count) {
    buffer_.reset(new Type[count_]);
    memset(buffer_.get(), 0, count_ * sizeof(Type));
  }

  Type* GetPointer() const { return buffer_.get(); }

  void Commit() {
    internal::CheckUserPointerWithCountHelper<
        sizeof(Type), MOJO_ALIGNOF(Type)>(user_pointer_.pointer_, count_);
    memcpy(user_pointer_.pointer_, buffer_.get(), count_ * sizeof(Type));
  }

 private:
  UserPointer<Type> user_pointer_;
  size_t count_;
  scoped_ptr<Type[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(UserPointerWriter);
};

// Implementation of |UserPointer<Type>::ReaderWriter|.
template <typename Type>
class UserPointerReaderWriter {
 public:
  UserPointerReaderWriter(UserPointer<Type> user_pointer, size_t count)
      : user_pointer_(user_pointer),
        count_(count) {
    internal::CheckUserPointerWithCountHelper<
        sizeof(Type), MOJO_ALIGNOF(Type)>(user_pointer_.pointer_, count_);
    buffer_.reset(new Type[count]);
    memcpy(buffer_.get(), user_pointer.pointer_, count * sizeof(Type));
  }

  Type* GetPointer() const { return buffer_.get(); }
  size_t GetCount() const { return count_; }

  void Commit() {
    internal::CheckUserPointerWithCountHelper<
        sizeof(Type), MOJO_ALIGNOF(Type)>(user_pointer_.pointer_, count_);
    memcpy(user_pointer_.pointer_, buffer_.get(), count_ * sizeof(Type));
  }

 private:
  UserPointer<Type> user_pointer_;
  size_t count_;
  scoped_ptr<Type[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(UserPointerReaderWriter);
};

// Represents a user pointer that will never be dereferenced by the system. The
// pointer value (i.e., the address) may be as a key for lookup, or may be a
// value that is only passed to the user at some point.
template <typename Type>
class UserPointerValue {
 public:
  UserPointerValue() : pointer_() {}
  explicit UserPointerValue(Type* pointer) : pointer_(pointer) {}
  ~UserPointerValue() {}

  // Returns the *value* of the pointer, which shouldn't be cast back to a
  // pointer and dereferenced.
  uintptr_t GetValue() const {
    return reinterpret_cast<uintptr_t>(pointer_);
  }

 private:
  Type* pointer_;
  // Allow copy and assignment.
};

// Provides a convenient way to make a |UserPointerValue<Type>|.
template <typename Type>
inline UserPointerValue<Type> MakeUserPointerValue(Type* pointer) {
  return UserPointerValue<Type>(pointer);
}

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MEMORY_H_
