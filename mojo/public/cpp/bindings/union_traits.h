// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_UNION_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_UNION_TRAITS_H_

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom union of type |MojomType|.
//
// Similar to StructTraits, each specialization of UnionTraits implements the
// following methods:
//   1. Getters for each field in the Mojom type.
//   2. Read() method.
//   3. [Optional] IsNull() and SetToNull().
//   4. [Optional] SetUpContext() and TearDownContext().
// Please see the documentation of StructTraits for details of these methods.
//
// Unlike StructTraits, there is one more method to implement:
//   5. A static GetTag() method indicating which field is the current active
//      field for serialization:
//
//        static |MojomType|DataView::Tag GetTag(const T& input);
//
//      During serialization, only the field getter corresponding to this tag
//      will be called.
//
template <typename MojomType, typename T>
struct UnionTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_UNION_TRAITS_H_
