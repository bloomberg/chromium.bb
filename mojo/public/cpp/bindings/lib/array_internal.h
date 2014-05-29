// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_

#include <new>
#include <vector>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"

namespace mojo {
template <typename T> class Array;
class String;

namespace internal {

template <typename T>
struct ArrayDataTraits {
  typedef T StorageType;
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

// What follows is code to support the serialization of Array_Data<T>. There
// are two interesting cases: arrays of primitives and arrays of objects.
// Arrays of objects are represented as arrays of pointers to objects.

template <typename T, bool kIsHandle> struct ArraySerializationHelper;

template <typename T>
struct ArraySerializationHelper<T, false> {
  typedef typename ArrayDataTraits<T>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
  }

  static bool DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       Message* message) {
    return true;
  }
};

template <>
struct ArraySerializationHelper<Handle, true> {
  typedef ArrayDataTraits<Handle>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles);

  static bool DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       Message* message);
};

template <typename H>
struct ArraySerializationHelper<H, true> {
  typedef typename ArrayDataTraits<H>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
    ArraySerializationHelper<Handle, true>::EncodePointersAndHandles(
        header, elements, handles);
  }

  static bool DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       Message* message) {
    return ArraySerializationHelper<Handle, true>::DecodePointersAndHandles(
        header, elements, message);
  }
};

template <typename P>
struct ArraySerializationHelper<P*, false> {
  typedef typename ArrayDataTraits<P*>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
    for (uint32_t i = 0; i < header->num_elements; ++i)
      Encode(&elements[i], handles);
  }

  static bool DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       Message* message) {
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (!Decode(&elements[i], message))
        return false;
    }
    return true;
  }
};

template <typename T>
class Array_Data {
 public:
  typedef ArrayDataTraits<T> Traits;
  typedef typename Traits::StorageType StorageType;
  typedef typename Traits::Ref Ref;
  typedef typename Traits::ConstRef ConstRef;
  typedef ArraySerializationHelper<T, IsHandle<T>::value> Helper;

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

  void EncodePointersAndHandles(std::vector<Handle>* handles) {
    Helper::EncodePointersAndHandles(&header_, storage(), handles);
  }

  bool DecodePointersAndHandles(Message* message) {
    return Helper::DecodePointersAndHandles(&header_, storage(), message);
  }

 private:
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

template <typename T, bool kIsMoveOnlyType> struct ArrayTraits {};

template <typename T> struct ArrayTraits<T, false> {
  typedef T StorageType;
  typedef typename std::vector<T>::reference RefType;
  typedef typename std::vector<T>::const_reference ConstRefType;
  static inline void Initialize(std::vector<T>* vec) {
  }
  static inline void Finalize(std::vector<T>* vec) {
  }
  static inline ConstRefType at(const std::vector<T>* vec, size_t offset) {
    return vec->at(offset);
  }
  static inline RefType at(std::vector<T>* vec, size_t offset) {
    return vec->at(offset);
  }
};

template <typename T> struct ArrayTraits<T, true> {
  struct StorageType {
    char buf[sizeof(T) + (8 - (sizeof(T) % 8)) % 8];  // Make 8-byte aligned.
  };
  typedef T& RefType;
  typedef const T& ConstRefType;
  static inline void Initialize(std::vector<StorageType>* vec) {
    for (size_t i = 0; i < vec->size(); ++i)
      new (vec->at(i).buf) T();
  }
  static inline void Finalize(std::vector<StorageType>* vec) {
    for (size_t i = 0; i < vec->size(); ++i)
      reinterpret_cast<T*>(vec->at(i).buf)->~T();
  }
  static inline ConstRefType at(const std::vector<StorageType>* vec,
                                size_t offset) {
    return *reinterpret_cast<const T*>(vec->at(offset).buf);
  }
  static inline RefType at(std::vector<StorageType>* vec, size_t offset) {
    return *reinterpret_cast<T*>(vec->at(offset).buf);
  }
};

template <> struct WrapperTraits<String, false> {
  typedef String_Data* DataType;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
