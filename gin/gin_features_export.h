// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_GIN_FEATURES_EXPORT_H_
#define GIN_GIN_FEATURES_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GIN_FEATURES_IMPLEMENTATION)
#define GIN_FEATURES_EXPORT __declspec(dllexport)
#else
#define GIN_FEATURES_EXPORT __declspec(dllimport)
#endif  // defined(GIN_FEATURES_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GIN_FEATURES_IMPLEMENTATION)
#define GIN_FEATURES_EXPORT __attribute__((visibility("default")))
#else
#define GIN_FEATURES_EXPORT
#endif  // defined(GIN_FEATURES_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define GIN_FEATURES_EXPORT
#endif

#endif  // GIN_GIN_FEATURES_EXPORT_H_
