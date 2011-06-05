// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_PPAPI_INTERFACES_H_
#define CHROME_RENDERER_CHROME_PPAPI_INTERFACES_H_
#pragma once

namespace chrome {

// The following 2 functions perform Chrome specific initialzation/
// uninitialization for PPAPI.
void InitializePPAPI();
void UninitializePPAPI();
}  // chrome

#endif  // CHROME_RENDERER_CHROME_PPAPI_INTERFACES_H_

