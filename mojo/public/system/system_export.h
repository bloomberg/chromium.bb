// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_SYSTEM_EXPORT_H_
#define MOJO_PUBLIC_SYSTEM_SYSTEM_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_SYSTEM_IMPLEMENTATION)
#define MOJO_SYSTEM_EXPORT __declspec(dllexport)
#else
#define MOJO_SYSTEM_EXPORT __declspec(dllimport)
#endif  // defined(GFX_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_SYSTEM_IMPLEMENTATION)
#define MOJO_SYSTEM_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_SYSTEM_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_SYSTEM_EXPORT
#endif

#endif  // MOJO_PUBLIC_SYSTEM_SYSTEM_EXPORT_H_
