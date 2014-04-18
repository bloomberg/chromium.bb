// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_

#include <new>

#include "mojo/public/cpp/bindings/buffer.h"
#include "mojo/public/cpp/bindings/interface.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"
#include "mojo/public/cpp/bindings/passable.h"

namespace mojo {
template <typename T> class Array;

namespace internal {

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

// What follows is code to support the serialization of Array_Data<T>. There
// are two interesting cases: arrays of primitives and arrays of objects.
// Arrays of objects are represented as arrays of pointers to objects.

template <typename T, bool kIsHandle> struct ArraySerializationHelper;

template <typename T>
struct ArraySerializationHelper<T, false> {
  typedef typename ArrayDataTraits<T>::StorageType ElementType;

  static size_t ComputeSizeOfElements(const ArrayHeader* header,
                                      const ElementType* elements) {
    return 0;
  }

  static void CloneElements(const ArrayHeader* header,
                            ElementType* elements,
                            Buffer* buf) {
  }

  static void ClearHandles(const ArrayHeader* header, ElementType* elements) {
  }

  static void CloseHandles(const ArrayHeader* header,
                           ElementType* elements) {
  }

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

  static size_t ComputeSizeOfElements(const ArrayHeader* header,
                                      const ElementType* elements) {
    return 0;
  }

  static void CloneElements(const ArrayHeader* header,
                            ElementType* elements,
                            Buffer* buf) {
  }

  static void ClearHandles(const ArrayHeader* header, ElementType* elements);

  static void CloseHandles(const ArrayHeader* header, ElementType* elements);

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

  static size_t ComputeSizeOfElements(const ArrayHeader* header,
                                      const ElementType* elements) {
    return 0;
  }

  static void CloneElements(const ArrayHeader* header,
                            ElementType* elements,
                            Buffer* buf) {
  }

  static void ClearHandles(const ArrayHeader* header, ElementType* elements) {
    ArraySerializationHelper<Handle, true>::ClearHandles(header, elements);
  }

  static void CloseHandles(const ArrayHeader* header, ElementType* elements) {
    ArraySerializationHelper<Handle, true>::CloseHandles(header, elements);
  }

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

  static size_t ComputeSizeOfElements(const ArrayHeader* header,
                                      const ElementType* elements) {
    size_t result = 0;
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (elements[i].ptr)
        result += elements[i].ptr->ComputeSize();
    }
    return result;
  }

  static void CloneElements(const ArrayHeader* header,
                            ElementType* elements,
                            Buffer* buf) {
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (elements[i].ptr)
        elements[i].ptr = elements[i].ptr->Clone(buf);
    }
  }

  static void ClearHandles(const ArrayHeader* header, ElementType* elements) {
  }

  static void CloseHandles(const ArrayHeader* header,
                           ElementType* elements) {
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (elements[i].ptr)
        elements[i].ptr->CloseHandles();
    }
  }

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
  typedef typename Traits::Wrapper Wrapper;
  typedef typename Traits::Ref Ref;
  typedef typename Traits::ConstRef ConstRef;
  typedef ArraySerializationHelper<T, TypeTraits<T>::kIsHandle> Helper;

  static Array_Data<T>* New(size_t num_elements, Buffer* buf,
                            Buffer::Destructor dtor = NULL) {
    size_t num_bytes = sizeof(Array_Data<T>) +
                       Traits::GetStorageSize(num_elements);
    return new (buf->Allocate(num_bytes, dtor)) Array_Data<T>(num_bytes,
                                                              num_elements);
  }

  static void Destructor(void* address) {
    static_cast<Array_Data*>(address)->CloseHandles();
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

  size_t ComputeSize() const {
    return Align(header_.num_bytes) +
        Helper::ComputeSizeOfElements(&header_, storage());
  }

  Array_Data<T>* Clone(Buffer* buf) {
    Array_Data<T>* clone = New(header_.num_elements, buf);
    memcpy(clone->storage(),
           storage(),
           header_.num_bytes - sizeof(Array_Data<T>));
    Helper::CloneElements(&clone->header_, clone->storage(), buf);

    // Zero-out handles in the original storage as they have been transferred
    // to the clone.
    Helper::ClearHandles(&header_, storage());
    return clone;
  }

  void CloseHandles() {
    Helper::CloseHandles(&header_, storage());
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

template <typename T, bool kIsObject, bool kIsHandle> struct ArrayTraits {};

// When T is an object type:
template <typename T> struct ArrayTraits<T, true, false> {
  typedef Array_Data<typename T::Data*> DataType;
  typedef const T& ConstRef;
  typedef T& Ref;
  static Buffer::Destructor GetDestructor() {
    return NULL;
  }
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

// When T is a primitive (non-bool) type:
template <typename T> struct ArrayTraits<T, false, false> {
  typedef Array_Data<T> DataType;
  typedef const T& ConstRef;
  typedef T& Ref;
  static Buffer::Destructor GetDestructor() {
    return NULL;
  }
  static T ToArrayElement(const T& value) {
    return value;
  }
  static Ref ToRef(T& data) { return data; }
  static ConstRef ToConstRef(const T& data) { return data; }
};

// When T is a bool type:
template <> struct ArrayTraits<bool, false, false> {
  typedef Array_Data<bool> DataType;
  typedef bool ConstRef;
  typedef ArrayDataTraits<bool>::Ref Ref;
  static Buffer::Destructor GetDestructor() {
    return NULL;
  }
  static bool ToArrayElement(const bool& value) {
    return value;
  }
  static Ref ToRef(const Ref& data) { return data; }
  static ConstRef ToConstRef(ConstRef data) { return data; }
};

// When T is a handle type:
template <typename H> struct ArrayTraits<H, false, true> {
  typedef Array_Data<H> DataType;
  typedef Passable<H> ConstRef;
  typedef AssignableAndPassable<H> Ref;
  static Buffer::Destructor GetDestructor() {
    return &DataType::Destructor;
  }
  static H ToArrayElement(const H& value) {
    return value;
  }
  static Ref ToRef(H& data) { return Ref(&data); }
  static ConstRef ToConstRef(const H& data) {
    return ConstRef(const_cast<H*>(&data));
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
