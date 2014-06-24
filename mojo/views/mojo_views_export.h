// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_VIEWS_MOJO_VIEWS_EXPORT_H_
#define MOJO_VIEWS_MOJO_VIEWS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_VIEWS_IMPLEMENTATION)
#define MOJO_VIEWS_EXPORT __declspec(dllexport)
#else
#define MOJO_VIEWS_EXPORT __declspec(dllimport)
#endif  // defined(MOJO_VIEWS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_VIEWS_IMPLEMENTATION)
#define MOJO_VIEWS_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_VIEWS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_VIEWS_EXPORT
#endif

#endif  // MOJO_VIEWS_MOJO_VIEWS_EXPORT_H_
