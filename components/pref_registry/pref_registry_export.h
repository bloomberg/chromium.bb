// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_EXPORT_H_
#define COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PREF_REGISTRY_IMPLEMENTATION)
#define PREF_REGISTRY_EXPORT __declspec(dllexport)
#else
#define PREF_REGISTRY_EXPORT __declspec(dllimport)
#endif  // defined(PREF_REGISTRY_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PREF_REGISTRY_IMPLEMENTATION)
#define PREF_REGISTRY_EXPORT __attribute__((visibility("default")))
#else
#define PREF_REGISTRY_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PREF_REGISTRY_EXPORT
#endif

#endif  // COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_EXPORT_H_
