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
//   void AcceptPoint(const geometry::Point& input) {
//     // With an explicit cast using the .To<> method.
//     gfx::Point pt = input.To<gfx::Point>();
//
//     mojo::AllocationScope scope;
//     // With an explicit cast using the static From() method.
//     geometry::Point output = geometry::Point::From(pt);
//   }
//
// More "direct" conversions can be enabled by adding the following macro to the
// TypeConverter specialization:
//  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
//
// To be exact, this amcro enables:
// - converting constructor:
//   T(const U& u, mojo::Buffer* buf = mojo::Buffer::current());
// - assignment operator:
//   T& operator=(const U& u);
// - conversion operator:
//   operator U() const;
//
// If the macro is added to TypeConverter<geometry::Point, gfx::Point>, for
// example, it is possible to write code like this:
//
//   void SomeFunction(const gfx::Point& pt);
//
//   void AcceptPoint(const geometry::Point& input) {
//     // Using the conversion operator.
//     SomeFunction(input);
//
//     mojo::AllocationScope scope;
//     // Using the converting constructor.
//     geometry::Point output_1(pt);
//
//     geometry::Point output_2;
//     // Using the assignment operator.
//     output_2 = pt;
//   }
//
// Although this macro is convenient, it makes conversions less obvious. Users
// may do conversions excessively without paying attention to the cost. So
// please use it wisely.
template <typename T, typename U> class TypeConverter {
  // static T ConvertFrom(const U& input, Buffer* buf);
  // static U ConvertTo(const T& input);

  // Maybe:
  // MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

}  // namespace mojo

#define MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION() \
    typedef void AllowImplicitTypeConversion

// Fails compilation if MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION() is not specified
// in TypeConverter<T, U>.
#define MOJO_INTERNAL_CHECK_ALLOW_IMPLICIT_TYPE_CONVERSION(T, U) \
    do { \
      typedef typename mojo::TypeConverter<T, U>::AllowImplicitTypeConversion \
          FailedIfNotAllowed; \
    } while (false)

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
