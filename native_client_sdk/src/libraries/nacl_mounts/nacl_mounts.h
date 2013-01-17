/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_NACL_MOUNTS_H_
#define LIBRARIES_NACL_MOUNTS_NACL_MOUNTS_H_

#include <ppapi/c/pp_instance.h>
#include <ppapi/c/ppb.h>

#include "nacl_mounts/kernel_wrap.h"
#include "utils/macros.h"

EXTERN_C_BEGIN


/** Initialize nacl_mounts.
 *
 * NOTE: If you initialize nacl_mounts with this constructor, you cannot
 * use any mounts that require PPAPI; e.g. persistent storage, etc.
 */
void nacl_mounts_init();

/** Initialize nacl_mounts with PPAPI support.
 *
 * Usage:
 *   PP_Instance instance;
 *   PPB_GetInterface get_interface;
 *   nacl_mounts_init(instance, get_interface);
 *
 * If you are using the PPAPI C interface:
 *   |instance| is passed to your instance in the DidCreate function.
 *   |get_interface| is passed to your module in the PPP_InitializeModule
 *   function.
 *
 * If you are using the PPAPI C++ interface:
 *   |instance| can be retrieved via the Instance::pp_instance() method.
 *   |get_interface| can be retrieved via
 *       pp::Module()::Get()->get_browser_interface()
 */
void nacl_mounts_init_ppapi(PP_Instance instance,
                            PPB_GetInterface get_interface);


/** Mount a new filesystem type.
 *
 * Some parameters are dependent on the filesystem type being mounted.
 *
 * The |data| parameter, if used, is always parsed as a string of comma
 * separated key-value pairs:
 *   e.g. "key1=param1,key2=param2"
 *
 *
 * filesystem types:
 *   "memfs": An in-memory filesystem.
 *     source: Unused.
 *     data: Unused.
 *
 *   "dev": A filesystem with various utility nodes. Some examples:
 *          "null": equivalent to /dev/null.
 *          "zero": equivalent to /dev/zero.
 *          "urandom": equivalent to /dev/urandom.
 *          "console[0-3]": logs to the JavaScript console with varying log
 *              levels.
 *          "tty": Posts a message to JavaScript, which will send a "message"
 *              event from this module's embed element.
 *     source: Unused.
 *     data: Unused.
 *
 *   "html5fs": A filesystem that uses PPAPI FileSystem interface, which can be
 *              read in JavaScript via the HTML5 FileSystem API. This mount
 *              provides the use of persistent storage. Please read the
 *              documentation in ppapi/c/ppb_file_system.h for more information.
 *     source: Unused.
 *     data: A string of parameters:
 *       "type": Which type of filesystem to mount. Valid values are
 *           "PERSISTENT" and "TEMPORARY". The default is "PERSISTENT".
 *       "expected_size": The expected file-system size. Note that this does
 *           not request quota -- you must do that from JavaScript.
 *
 *
 * @param[in] source Depends on the filesystem type. See above.
 * @param[in] target The absolute path to mount the filesystem.
 * @param[in] filesystemtype The name of the filesystem type to mount. See
 *     above for examples.
 * @param[in] mountflags Unused.
 * @param[in] data Depends on the filesystem type. See above.
 * @return 0 on success, -1 on failure (with errno set).
 */
int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void *data) NOTHROW;

EXTERN_C_END

#endif  // LIBRARIES_NACL_MOUNTS_NACL_MOUNTS_H_
