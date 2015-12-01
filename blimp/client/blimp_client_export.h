// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_BLIMP_CLIENT_EXPORT_H_
#define BLIMP_CLIENT_BLIMP_CLIENT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BLIMP_CLIENT_IMPLEMENTATION)
#define BLIMP_CLIENT_EXPORT __declspec(dllexport)
#else
#define BLIMP_CLIENT_EXPORT __declspec(dllimport)
#endif  // defined(BLIMP_CLIENT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(BLIMP_CLIENT_IMPLEMENTATION)
#define BLIMP_CLIENT_EXPORT __attribute__((visibility("default")))
#else
#define BLIMP_CLIENT_EXPORT
#endif  // defined(BLIMP_CLIENT_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define BLIMP_CLIENT_EXPORT
#endif

#endif  // BLIMP_CLIENT_BLIMP_CLIENT_EXPORT_H_
