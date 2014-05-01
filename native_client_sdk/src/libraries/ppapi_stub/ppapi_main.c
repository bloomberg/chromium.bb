/*
 * Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>

#include "irt_syscalls.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppp.h"

struct PP_StartFunctions {
  int32_t (*PPP_InitializeModule)(PP_Module module_id,
                                  PPB_GetInterface get_browser_interface);
  void (*PPP_ShutdownModule)();
  const void* (*PPP_GetInterface)(const char* interface_name);
};

struct PP_ThreadFunctions {
  /*
   * This is a cut-down version of pthread_create()/pthread_join().
   * We omit thread creation attributes and the thread's return value.
   *
   * We use uintptr_t as the thread ID type because pthread_t is not
   * part of the stable ABI; a user thread library might choose an
   * arbitrary size for its own pthread_t.
   */
  int (*thread_create)(uintptr_t* tid,
                       void (*func)(void* thread_argument),
                       void* thread_argument);
  int (*thread_join)(uintptr_t tid);
};

#define NACL_IRT_PPAPIHOOK_v0_1 "nacl-irt-ppapihook-0.1"
struct nacl_irt_ppapihook {
  int (*ppapi_start)(const struct PP_StartFunctions*);
  void (*ppapi_register_thread_creator)(const struct PP_ThreadFunctions*);
};


static int thread_create(uintptr_t *tid,
                         void (*func)(void *thread_argument),
                         void *thread_argument) {
  /*
   * We know that newlib and glibc use a small pthread_t type, so we
   * do not need to wrap pthread_t values.
   */
  return pthread_create((pthread_t *) tid, NULL,
                        (void *(*)(void *thread_argument)) func,
                        thread_argument);
}

static int thread_join(uintptr_t tid) {
  return pthread_join((pthread_t) tid, NULL);
}

/*
 * These are dangling references to functions that the application must define.
 */
static const struct PP_StartFunctions ppapi_app_start_callbacks = {
  PPP_InitializeModule,
  PPP_ShutdownModule,
  PPP_GetInterface
};

const static struct PP_ThreadFunctions thread_funcs = {
  thread_create,
  thread_join
};

static void fatal_error(const char *message) {
  write(2, message, strlen(message));
  _exit(127);
}

/*
 * We cannot tell at link time whether the application uses PPB_Audio,
 * because of the way that PPAPI is defined via runtime interface
 * query rather than a set of static functions.  This means that we
 * register the audio thread functions unconditionally.  This adds the
 * small overhead of pulling in pthread_create() even if the
 * application does not use PPB_Audio or libpthread.
 *
 * If an application developer wants to avoid that cost, they can
 * override this function with an empty definition.
 */
void __nacl_register_thread_creator(const struct nacl_irt_ppapihook *hooks) {
  hooks->ppapi_register_thread_creator(&thread_funcs);
}

int PpapiPluginStart(const struct PP_StartFunctions *funcs) {
  struct nacl_irt_ppapihook hooks;
  if (sizeof(hooks) != __nacl_irt_query(NACL_IRT_PPAPIHOOK_v0_1,
                                        &hooks, sizeof(hooks))) {
    fatal_error("PpapiPluginStart: PPAPI hooks not found\n");
  }

  __nacl_register_thread_creator(&hooks);
  return hooks.ppapi_start(funcs);
}


/*
 * The application's main (or the one supplied in this library) calls this
 * to start the PPAPI world.
 */
int PpapiPluginMain(void) {
  return PpapiPluginStart(&ppapi_app_start_callbacks);
}
