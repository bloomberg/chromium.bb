// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RANDOM_H_
#define CHROME_COMMON_RANDOM_H_
#pragma once

#include <string>

// Generate128BitRandomBase64String returns a string of length 24 containing
// cryptographically strong random data encoded in base64.
std::string Generate128BitRandomBase64String();

#endif  // CHROME_COMMON_RANDOM_H_
