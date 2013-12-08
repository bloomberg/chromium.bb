// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include <new>

#include "mojo/public/bindings/lib/buffer.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
template <typename T> class Array;

namespace internal {
template <typename T> class Array_Data;

#pragma pack(push, 1)

struct StructHeader {
  uint32_t num_bytes;
  uint32_t num_fields;
};
MOJO_COMPILE_ASSERT(sizeof(StructHeader) == 8, bad_sizeof_StructHeader);

struct ArrayHeader {
  uint32_t num_bytes;
  uint32_t num_elements;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayHeader) == 8, bad_sizeof_ArrayHeader);

template <typename T>
union StructPointer {
  uint64_t offset;
  T* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StructPointer<char>) == 8, bad_sizeof_StructPointer);

template <typename T>
union ArrayPointer {
  uint64_t offset;
  Array_Data<T>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayPointer<char>) == 8, bad_sizeof_ArrayPointer);

union StringPointer {
  uint64_t offset;
  Array_Data<char>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StringPointer) == 8, bad_sizeof_StringPointer);

#pragma pack(pop)

template <typename T>
void ResetIfNonNull(T* ptr) {
  if (ptr)
    *ptr = T();
}

template <typename T>
T FetchAndReset(T* ptr) {
  T temp = *ptr;
  *ptr = T();
  return temp;
}

template <typename T>
class WrapperHelper {
 public:
  static const T Wrap(const typename T::Data* data) {
    return T(typename T::Wrap(), const_cast<typename T::Data*>(data));
  }
  static typename T::Data* Unwrap(const T& object) {
    return const_cast<typename T::Data*>(object.data_);
  }
};

template <typename Data>
inline const typename Data::Wrapper Wrap(const Data* data) {
  return WrapperHelper<typename Data::Wrapper>::Wrap(data);
}

template <typename T>
inline typename T::Data* Unwrap(const T& object) {
  return WrapperHelper<T>::Unwrap(object);
}

template <typename T> struct TypeTraits {
  static const bool kIsObject = true;
};
template <> struct TypeTraits<bool> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<char> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int8_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int16_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int32_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int64_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint8_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint16_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint32_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint64_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<float> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<double> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<Handle> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<MessagePipeHandle> {
  static const bool kIsObject = false;
};

template <typename T>
struct ArrayDataTraits {
  typedef T StorageType;
  typedef Array<T> Wrapper;
  typedef T& Ref;
  typedef T const& ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return sizeof(StorageType) * num_elements;
  }
  static Ref ToRef(StorageType* storage, size_t offset) {
    return storage[offset];
  }
  static ConstRef ToConstRef(const StorageType* storage, size_t offset) {
    return storage[offset];
  }
};

template <typename P>
struct ArrayDataTraits<P*> {
  typedef StructPointer<P> StorageType;
  typedef Array<typename P::Wrapper> Wrapper;
  typedef P*& Ref;
  typedef P* const& ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return sizeof(StorageType) * num_elements;
  }
  static Ref ToRef(StorageType* storage, size_t offset) {
    return storage[offset].ptr;
  }
  static ConstRef ToConstRef(const StorageType* storage, size_t offset) {
    return storage[offset].ptr;
  }
};

// Specialization of Arrays for bools, optimized for space. It has the
// following differences from a generalized Array:
// * Each element takes up a single bit of memory.
// * Accessing a non-const single element uses a helper class |BitRef|, which
// emulates a reference to a bool.
template <>
struct ArrayDataTraits<bool> {
  // Helper class to emulate a reference to a bool, used for direct element
  // access.
  class BitRef {
   public:
    ~BitRef();
    BitRef& operator=(bool value);
    BitRef& operator=(const BitRef& value);
    operator bool() const;
   private:
    friend struct ArrayDataTraits<bool>;
    BitRef(uint8_t* storage, uint8_t mask);
    BitRef();
    uint8_t* storage_;
    uint8_t mask_;
  };

  typedef uint8_t StorageType;
  typedef Array<bool> Wrapper;
  typedef BitRef Ref;
  typedef bool ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return ((num_elements + 7) / 8);
  }
  static BitRef ToRef(StorageType* storage, size_t offset) {
    return BitRef(&storage[offset / 8], 1 << (offset % 8));
  }
  static bool ToConstRef(const StorageType* storage, size_t offset) {
    return (storage[offset / 8] & (1 << (offset % 8))) != 0;
  }
};

template <typename T> class ObjectTraits {};

template <typename T>
class Array_Data {
 public:
  typedef ArrayDataTraits<T> Traits;
  typedef typename Traits::StorageType StorageType;
  typedef typename Traits::Wrapper Wrapper;
  typedef typename Traits::Ref Ref;
  typedef typename Traits::ConstRef ConstRef;

  static Array_Data<T>* New(size_t num_elements, Buffer* buf) {
    size_t num_bytes = sizeof(Array_Data<T>) +
                       Traits::GetStorageSize(num_elements);
    return new (buf->Allocate(num_bytes)) Array_Data<T>(num_bytes,
                                                        num_elements);
  }

  size_t size() const { return header_.num_elements; }

  Ref at(size_t offset) {
    assert(offset < static_cast<size_t>(header_.num_elements));
    return Traits::ToRef(storage(), offset);
  }

  ConstRef at(size_t offset) const {
    assert(offset < static_cast<size_t>(header_.num_elements));
    return Traits::ToConstRef(storage(), offset);
  }

  StorageType* storage() {
    return reinterpret_cast<StorageType*>(
        reinterpret_cast<char*>(this) + sizeof(*this));
  }

  const StorageType* storage() const {
    return reinterpret_cast<const StorageType*>(
        reinterpret_cast<const char*>(this) + sizeof(*this));
  }

 private:
  friend class internal::ObjectTraits<Array_Data<T> >;

  Array_Data(size_t num_bytes, size_t num_elements) {
    header_.num_bytes = static_cast<uint32_t>(num_bytes);
    header_.num_elements = static_cast<uint32_t>(num_elements);
  }
  ~Array_Data() {}

  internal::ArrayHeader header_;

  // Elements of type internal::ArrayDataTraits<T>::StorageType follow.
};
MOJO_COMPILE_ASSERT(sizeof(Array_Data<char>) == 8, bad_sizeof_Array_Data);

// UTF-8 encoded
typedef Array_Data<char> String_Data;

template <typename T, bool kIsObject> struct ArrayTraits {};

template <typename T> struct ArrayTraits<T, true> {
  typedef Array_Data<typename T::Data*> DataType;
  typedef const T& ConstRef;
  typedef T& Ref;
  static typename T::Data* ToArrayElement(const T& value) {
    return Unwrap(value);
  }
  // Something sketchy is indeed happening here...
  static Ref ToRef(typename T::Data*& data) {
    return *reinterpret_cast<T*>(&data);
  }
  static ConstRef ToConstRef(typename T::Data* const& data) {
    return *reinterpret_cast<const T*>(&data);
  }
};

template <typename T> struct ArrayTraits<T, false> {
  typedef Array_Data<T> DataType;
  typedef const T& ConstRef;
  typedef T& Ref;
  static T ToArrayElement(const T& value) {
    return value;
  }
  static Ref ToRef(T& data) { return data; }
  static ConstRef ToConstRef(const T& data) { return data; }
};

template <> struct ArrayTraits<bool, false> {
  typedef Array_Data<bool> DataType;
  typedef bool ConstRef;
  typedef ArrayDataTraits<bool>::Ref Ref;
  static bool ToArrayElement(const bool& value) {
    return value;
  }
  static Ref ToRef(const Ref& data) { return data; }
  static ConstRef ToConstRef(ConstRef data) { return data; }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
