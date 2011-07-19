// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_API_H_
#define CONTENT_COMMON_CONTENT_API_H_
#pragma once

// Defines CONTENT_API so that funtionality implemented by the content module
// can be exported to consumers, and CONTENT_TEST that allows unit tests to
// access features not intended to be used directly by real consumers.
#if defined(WIN32) && defined(CONTENT_DLL)
#if defined(CONTENT_IMPLEMENTATION)
#define CONTENT_API __declspec(dllexport)
#define CONTENT_TEST __declspec(dllexport)
#else
#define CONTENT_API __declspec(dllimport)
#define CONTENT_TEST __declspec(dllimport)
#endif  // defined(CONTENT_IMPLEMENTATION)
#else
#define CONTENT_API
#define CONTENT_TEST
#endif

#endif  // CONTENT_COMMON_CONTENT_API_H_
