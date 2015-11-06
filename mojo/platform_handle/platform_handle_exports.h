// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_EXPORTS_H_
#define MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_EXPORTS_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PLATFORM_HANDLE_IMPLEMENTATION)
#define PLATFORM_HANDLE_EXPORT __declspec(dllexport)
#else
#define PLATFORM_HANDLE_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(PLATFORM_HANDLE_IMPLEMENTATION)
#define PLATFORM_HANDLE_EXPORT __attribute__((visibility("default")))
#else
#define PLATFORM_HANDLE_EXPORT
#endif

#endif  // defined(WIN32)

#else  // !defined(COMPONENT_BUILD)

#define PLATFORM_HANDLE_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_EXPORTS_H_
