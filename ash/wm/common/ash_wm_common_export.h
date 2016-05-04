// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_ASH_WM_COMMON_EXPORT_H_
#define ASH_WM_COMMON_ASH_WM_COMMON_EXPORT_H_

// Defines ASH_WM_COMMON_EXPORT so that functionality implemented by the Ash
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(ASH_WM_COMMON_IMPLEMENTATION)
#define ASH_WM_COMMON_EXPORT __declspec(dllexport)
#else
#define ASH_WM_COMMON_EXPORT __declspec(dllimport)
#endif  // defined(ASH_WM_COMMON_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(ASH_WM_COMMON_IMPLEMENTATION)
#define ASH_WM_COMMON_EXPORT __attribute__((visibility("default")))
#else
#define ASH_WM_COMMON_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define ASH_WM_COMMON_EXPORT
#endif

#endif  // ASH_WM_COMMON_ASH_WM_COMMON_EXPORT_H_
