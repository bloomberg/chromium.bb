// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_AEDESC_H_
#define BASE_SCOPED_AEDESC_H_
#pragma once

#import <CoreServices/CoreServices.h>

#include "base/basictypes.h"

// The scoped_aedesc is used to scope AppleEvent descriptors.  On creation,
// it will store a NULL descriptor.  On destruction, it will dispose of the
// descriptor.
//
// This class is parameterized for additional type safety checks.  You can use
// the generic AEDesc type by not providing a template parameter:
//  scoped_aedesc<> desc;
template <typename AEDescType = AEDesc>
class scoped_aedesc {
 public:
  scoped_aedesc() {
    AECreateDesc(typeNull, NULL, 0, &desc_);
  }

  ~scoped_aedesc() {
    AEDisposeDesc(&desc_);
  }

  // Used for in parameters.
  operator const AEDescType*() {
    return &desc_;
  }

  // Used for out parameters.
  AEDescType* OutPointer() {
    return &desc_;
  }

 private:
  AEDescType desc_;

  DISALLOW_COPY_AND_ASSIGN(scoped_aedesc);
};

#endif  // BASE_SCOPED_AEDESC_H_
