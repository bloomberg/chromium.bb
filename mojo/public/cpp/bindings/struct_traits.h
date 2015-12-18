// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom struct of type |MojomType|.
//
// Each specialization must implement a few things:
//
//   1. Static getters for each field in the Mojom type. These should be
//      of the form:
//
//        static <return type> <field name>(const T&)
//
//      and should return a serializable form of the named field as extracted
//      from the referenced |T| instance.
//
//   2. A static Read method to initialize a new |T| from a MojomType::Reader:
//
//        static bool Read(MojomType::Reader r, T* out);
//
//      The generated MojomType::Reader type provides a convenient, inexpensive
//      view of a serialized struct's field data.
//
template <typename MojomType, typename T>
struct StructTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
