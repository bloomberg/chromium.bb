// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_CRYPTO_API_H_
#define CRYPTO_CRYPTO_API_H_
#pragma once

#if defined(CRYPTO_DLL)
#if defined(WIN32)
#if defined(CRYPTO_IMPLEMENTATION)
#define CRYPTO_API __declspec(dllexport)
#else
#define CRYPTO_API __declspec(dllimport)
#endif  // defined(CRYPTO_IMPLEMENTATION)
#else
#define CRYPTO_API __attribute__((visibility("default")))
#endif  // defined(WIN32)
#else
#define CRYPTO_API
#endif

#endif  // CRYPTO_CRYPTO_API_H_
