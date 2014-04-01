// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_

namespace mojo {

// Specialize to perform type conversion for Mojom-defined structs and arrays.
// Here, T is the Mojom-defined struct or array, and U is some other non-Mojom
// struct or array type.
//
// EXAMPLE:
//
// Suppose you have the following Mojom-defined struct:
//
//   module geometry {
//   struct Point {
//     int32 x;
//     int32 y;
//   };
//   }
//
// Now, imagine you wanted to write a TypeConverter specialization for
// gfx::Point. It might look like this:
//
//   namespace mojo {
//   template <>
//   class TypeConverter<geometry::Point, gfx::Point> {
//    public:
//     static geometry::Point ConvertFrom(const gfx::Point& input,
//                                        Buffer* buf) {
//       geometry::Point::Builder builder(buf);
//       builder.set_x(input.x());
//       builder.set_y(input.y());
//       return builder.Finish();
//     }
//     static gfx::Point ConvertTo(const geometry::Point& input) {
//       return gfx::Point(input.x(), input.y());
//     }
//   };
//   }
//
// With the above TypeConverter defined, it is possible to write code like this:
//
//   void SomeFunction(const gfx::Point& pt);
//
//   void AcceptPoint(const geometry::Point& input) {
//     // With an explicit cast using the .To<> method.
//     gfx::Point pt = input.To<gfx::Point>();
//
//     // With an implicit copy conversion:
//     SomeFunction(input);
//
//     mojo::AllocationScope scope;
//     // With an implicit copy conversion:
//     geometry::Point output = pt;
//   }
//
template <typename T, typename U> class TypeConverter {
  // static T ConvertFrom(const U& input, Buffer* buf);
  // static U ConvertTo(const T& input);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
