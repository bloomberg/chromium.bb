// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/nacl_io.h"

#include <stdlib.h>
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"

void nacl_io_init() {
  ki_init(NULL);
}

void nacl_io_init_ppapi(PP_Instance instance, PPB_GetInterface get_interface) {
  ki_init_ppapi(NULL, instance, get_interface);
}

int nacl_io_register_fs_type(const char* fs_type, fuse_operations* fuse_ops) {
  return ki_get_proxy()->RegisterFsType(fs_type, fuse_ops);
}

int nacl_io_unregister_fs_type(const char* fs_type) {
  return ki_get_proxy()->UnregisterFsType(fs_type);
}

int nacl_io_register_exit_handler(nacl_io_exit_handler_t exit_handler,
                                  void* user_data) {
  return ki_get_proxy()->RegisterExitHandler(exit_handler, user_data);
}
