// Copyright $YEAR The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef $HEADER_GUARD
#define $HEADER_GUARD

#include "mojo/public/bindings/lib/bindings.h"

namespace $NAMESPACE {
$FORWARDS
#pragma pack(push, 1)

class $CLASS {
 public:
  static $CLASS* New(mojo::Buffer* buf);

$SETTERS

$GETTERS

 private:
  friend class mojo::internal::ObjectTraits<$CLASS>;

  $CLASS();
  ~$CLASS();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
$FIELDS
};

MOJO_COMPILE_ASSERT(sizeof($CLASS) == $SIZE, bad_sizeof_$CLASS);

#pragma pack(pop)

}  // namespace $NAMESPACE

#endif  // $HEADER_GUARD
