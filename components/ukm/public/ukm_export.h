// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_PUBLIC_UKM_EXPORT_H_
#define COMPONENTS_UKM_PUBLIC_UKM_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(UKM_IMPLEMENTATION)
#define UKM_EXPORT __declspec(dllexport)
#else
#define UKM_EXPORT __declspec(dllimport)
#endif  // defined(UKM_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(UKM_IMPLEMENTATION)
#define UKM_EXPORT __attribute__((visibility("default")))
#else
#define UKM_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define UKM_EXPORT
#endif

#endif  // COMPONENTS_UKM_PUBLIC_UKM_EXPORT_H_
