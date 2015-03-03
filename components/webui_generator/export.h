// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WUG_EXPORT_H_
#define COMPONENTS_WUG_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(WUG_IMPLEMENTATION)
#define WUG_EXPORT __declspec(dllexport)
#else
#define WUG_EXPORT __declspec(dllimport)
#endif  // defined(WUG_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(WUG_IMPLEMENTATION)
#define WUG_EXPORT __attribute__((visibility("default")))
#else
#define WUG_EXPORT
#endif  // defined(WUG_IMPLEMENTATION)

#endif  // defined(WIN32)

#else  // defined(COMPONENT_BUILD)

#define WUG_EXPORT

#endif

#endif  // COMPONENTS_WUG_EXPORT_H_
