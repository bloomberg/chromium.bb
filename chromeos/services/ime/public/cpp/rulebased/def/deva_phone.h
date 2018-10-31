// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_DEVA_PHONE_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_DEVA_PHONE_H_

namespace deva_phone {

// The transform rules for the deva_phone IME.
extern const char* kTransforms[];

// The length of the transform rule for the deva_phone IME.
extern const unsigned int kTransformsLen;

// The history prune regexp for the deva_phone IME.
extern const char* kHistoryPrune;

// The id of the deva_phone IME.
extern const char* kId;

}  // namespace deva_phone

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_DEVA_PHONE_H_
