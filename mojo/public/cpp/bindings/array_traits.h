// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom array.
//
// Usually you would like to do a partial specialization for an array/vector
// template. Imagine you want to specialize it for CustomArray<>, you need to
// implement:
//
//   template <typename T>
//   struct ArrayTraits<CustomArray<T>> {
//     using Element = T;
//
//     // These two methods are optional. Please see comments in struct_traits.h
//     static bool IsNull(const CustomArray<T>& input);
//     static void SetToNull(CustomArray<T>* output);
//
//     static size_t GetSize(const CustomArray<T>& input);
//
//     static T* GetData(CustomArray<T>& input);
//     static const T* GetData(const CustomArray<T>& input);
//
//     static T& GetAt(CustomArray<T>& input, size_t index);
//     static const T& GetAt(const CustomArray<T>& input, size_t index);
//
//     static void Resize(CustomArray<T>& input, size_t size);
//   };
//
template <typename T>
struct ArrayTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
