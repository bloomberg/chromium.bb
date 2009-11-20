// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"

#if NACL_OSX
#include <crt_externs.h>
#endif

#define NACL_NO_INLINE

EXTERN_C_BEGIN
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/expiration.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"
EXTERN_C_END

int verbosity = 0;

#ifdef __GNUC__

/*
 * GDB's canonical overlay managment routine.
 * We need its symbol in the symbol table so don't inline it.
 * TODO(dje): add some explanation for the non-GDB person.
 */

static void __attribute__ ((noinline)) _ovly_debug_event (void) {
  /*
   * The asm volatile is here as instructed by the GCC docs.
   * It's not enough to declare a function noinline.
   * GCC will still look inside the function to see if it's worth calling.
   */
  asm volatile ("");
}

#endif

static void StopForDebuggerInit(const struct NaClApp *state) {
  /* Put xlate_base in a place where gdb can find it.  */
  nacl_global_xlate_base = state->mem_start;

#ifdef __GNUC__
  _ovly_debug_event();
#endif
}

int SelMain(const int desc, const NaClHandle handle) {
  return 0;
#if 0  // TODO(gregoryd): enable when the service runtime is ready
  char *av[1];
  int ac = 1;

  char                          **envp;
  struct NaClApp                state;
  char                          *nacl_file = 0;
  int                           main_thread_only = 1;
  int                           export_addr_to = -2;

  struct NaClApp                *nap;

  NaClErrorCode                 errcode;

  int                           ret_code = 1;
#if NACL_OSX
  // Mac dynamic libraries cannot access the environ variable directly.
  envp = *_NSGetEnviron();
#else
  extern char                   **environ;
  envp = environ;
#endif

  NaClAllModulesInit();

  /* used to be -P */
  NaClSrpcFileDescriptor = desc;
  /* used to be -X */
  export_addr_to = desc;

  /* to be passed to NaClMain, eventually... */
  av[0] = const_cast<char*>("NaClMain");

  if (!NaClAppCtor(&state)) {
    fprintf(stderr, "Error while constructing app state\n");
    goto done_file_dtor;
  }

  state.restrict_to_main_thread = main_thread_only;

  nap = &state;
  errcode = LOAD_OK;

  /* import IMC handle - used to be "-i" */
  NaClAddImcHandle(nap, handle, desc);

  /*
   * in order to report load error to the browser plugin through the
   * secure command channel, we do not immediate jump to cleanup code
   * on error.  rather, we continue processing (assuming earlier
   * errors do not make it inappropriate) until the secure command
   * channel is set up, and then bail out.
   */

  /*
   * Ensure this operating system platform is supported.
   */
  if (!NaClOsIsSupported()) {
    errcode = LOAD_UNSUPPORTED_OS_PLATFORM;
    nap->module_load_status = errcode;
    fprintf(stderr, "Error while loading \"%s\": %s\n",
            nacl_file,
            NaClErrorString(errcode));
  }

  /* Give debuggers a well known point at which xlate_base is known.  */
  StopForDebuggerInit(&state);

  /*
   * If export_addr_to is set to a non-negative integer, we create a
   * bound socket and socket address pair and bind the former to
   * descriptor 3 and the latter to descriptor 4.  The socket address
   * is written out to the export_addr_to descriptor.
   *
   * The service runtime also accepts a connection on the bound socket
   * and spawns a secure command channel thread to service it.
   *
   * If export_addr_to is -1, we only create the bound socket and
   * socket address pair, and we do not export to an IMC socket.  This
   * use case is typically only used in testing, where we only "dump"
   * the socket address to stdout or similar channel.
   */
  if (-2 < export_addr_to) {
    NaClCreateServiceSocket(nap);
    if (0 <= export_addr_to) {
      NaClSendServiceAddressTo(nap, export_addr_to);
      /*
       * NB: spawns a thread that uses the command channel.  we do
       * this after NaClAppLoadFile so that NaClApp object is more
       * fully populated.  Hereafter any changes to nap should be done
       * while holding locks.
       */
      NaClSecureCommandChannel(nap);
    }
  }

  NaClXMutexLock(&nap->mu);
  nap->module_load_status = LOAD_OK;
  NaClXCondVarBroadcast(&nap->cv);
  NaClXMutexUnlock(&nap->mu);

  if (NULL != nap->secure_channel) {
    /*
     * wait for start_module RPC call on secure channel thread.
     */
    NaClWaitForModuleStartStatusCall(nap);
  }

  /*
   * error reporting done; can quit now if there was an error earlier.
   */
  if (LOAD_OK != errcode) {
    goto done;
  }

  /*
   * only nap->ehdrs.e_entry is usable, no symbol table is
   * available.
   */
  if (!NaClCreateMainThread(nap,
                            ac,
                            av,
                            envp)) {
    fprintf(stderr, "creating main thread failed\n");
    goto done;
  }

  ret_code = NaClWaitForMainThreadToExit(nap);

  /*
   * exit_group or equiv kills any still running threads while module
   * addr space is still valid.  otherwise we'd have to kill threads
   * before we clean up the address space.
   */
  return ret_code;

 done:
  fflush(stdout);

  NaClAppDtor(&state);

 done_file_dtor:
  fflush(stdout);

  NaClAllModulesFini();

  return ret_code;
#endif
}

