// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMPILER_SPECIFIC_H_
#define GPU_COMMAND_BUFFER_COMMON_COMPILER_SPECIFIC_H_
#pragma once

// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void foo() OVERRIDE;
#ifndef OVERRIDE
#ifdef _MSC_VER
#define OVERRIDE override
#elif defined(__clang__)
#define OVERRIDE override
#else
#define OVERRIDE
#endif
#endif

#endif  // GPU_COMMAND_BUFFER_COMMON_COMPILER_SPECIFIC_H_
