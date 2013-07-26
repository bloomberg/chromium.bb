/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PPAPI_SIMPLE_PS_MAIN_H_
#define PPAPI_SIMPLE_PS_MAIN_H_

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_event.h"

EXTERN_C_BEGIN

typedef int (*PSMainFunc_t)(int argc, char *argv[]);

/**
 * PSMainCreate
 *
 * Constructs an instance SimpleInstance and configures it to call into
 * the provided "main" function.
 */
void* PSMainCreate(PP_Instance inst, PSMainFunc_t func);


/**
 * PPAPI_SIMPLE_REGISTER_MAIN
 *
 * Constructs a PSInstance object and configures it to use call the provided
 * 'main' function on its own thread once initialization is complete.
 */
#define PPAPI_SIMPLE_REGISTER_MAIN(main) \
  PPAPI_SIMPLE_USE_MAIN(PSMainCreate, main)

EXTERN_C_END

#endif  /* PPAPI_SIMPLE_PS_MAIN_H_ */
