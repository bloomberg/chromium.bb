// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMMON_API_H_
#define CHROME_COMMON_COMMON_API_H_
#pragma once

// Defines COMMON_API so that funtionality implemented by the common module can
// be exported to consumers, and COMMON_TEST that allows unit tests to access
// features not intended to be used directly by real consumers.

#if defined(WIN32) && defined(COMMON_DLL)
#if defined(COMMON_IMPLEMENTATION)
#define COMMON_API __declspec(dllexport)
#define COMMON_TEST __declspec(dllexport)
#else
#define COMMON_API __declspec(dllimport)
#define COMMON_TEST __declspec(dllimport)
#endif  // defined(COMMON_IMPLEMENTATION)
#else
#define COMMON_API
#define COMMON_TEST
#endif

#endif  // CHROME_COMMON_COMMON_API_H_
