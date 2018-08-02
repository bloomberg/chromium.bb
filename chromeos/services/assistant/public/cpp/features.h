// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_

namespace base {
struct Feature;
}  // namespace base

namespace chromeos {
namespace assistant {

// Enables DSP for hotword detection.
extern const base::Feature kEnableDspHotword;

bool IsDspHotwordEnabled();

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
