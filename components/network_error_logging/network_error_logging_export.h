// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_EXPORT_H_
#define COMPONENTS_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(NETWORK_ERROR_LOGGING_IMPLEMENTATION)
#define NETWORK_ERROR_LOGGING_EXPORT __declspec(dllexport)
#else
#define NETWORK_ERROR_LOGGING_EXPORT __declspec(dllimport)
#endif

#else  // defined(WIN32)

#if defined(NETWORK_ERROR_LOGGING_IMPLEMENTATION)
#define NETWORK_ERROR_LOGGING_EXPORT __attribute__((visibility("default")))
#else
#define NETWORK_ERROR_LOGGING_EXPORT
#endif

#endif  // defined(WIN32)
#else   // defined(COMPONENT_BUILD)

#define NETWORK_ERROR_LOGGING_EXPORT

#endif

#endif  // COMPONENTS_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_EXPORT_H_
