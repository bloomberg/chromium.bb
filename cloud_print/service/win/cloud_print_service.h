// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_CLOUD_PRINT_SERVICE_H_
#define CLOUD_PRINT_SERVICE_CLOUD_PRINT_SERVICE_H_

#ifndef STRICT
#define STRICT
#endif

#define _ATL_FREE_THREADED

#define _ATL_NO_AUTOMATIC_NAMESPACE

// Service has no COM objects. http://support.microsoft.com/kb/2480736
#define _ATL_NO_COM_SUPPORT

// some CString constructors will be explicit
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#endif  // CLOUD_PRINT_SERVICE_CLOUD_PRINT_SERVICE_H_

