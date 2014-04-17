/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef NACL_IO_DEMO_H_
#define NACL_IO_DEMO_H_

#include <stdarg.h>
#include "ppapi/c/pp_var.h"
#include "sdk_util/macros.h"  // for PRINTF_LIKE

struct PP_Var CStrToVar(const char* str);
char* VprintfToNewString(const char* format, va_list args) PRINTF_LIKE(1, 0);
char* PrintfToNewString(const char* format, ...) PRINTF_LIKE(1, 2);
struct PP_Var PrintfToVar(const char* format, ...) PRINTF_LIKE(1, 2);
uint32_t VarToCStr(struct PP_Var var, char* buffer, uint32_t length);

#endif /* NACL_IO_DEMO_H_ */
