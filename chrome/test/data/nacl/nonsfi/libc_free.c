/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This is a minimal NaCl program without libc. It uses NaCl's stable IRT ABI.
 */

#include <stdint.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"

static struct nacl_irt_basic __libnacl_irt_basic;
static TYPE_nacl_irt_query __nacl_irt_query;
static struct PPP_Instance_1_0 ppp_instance;
static struct PPP_Messaging_1_0 ppp_messaging;
static const struct PPB_Messaging_1_0* ppb_messaging;

/*
 * To support 64bit binary, we declare Elf_auxv_t by using uintptr_t.
 * See also native_client/src/include/elf32.h.
 */
typedef struct {
  uintptr_t a_type;
  union {
    uintptr_t a_val;
  } a_un;
} Elf_auxv_t;

/*
 * This is simiplar to the one in
 * native_client/src/untrusted/nacl/nacl_startup.h, but also supports 64 bit
 * pointers.
 */
static Elf_auxv_t* nacl_startup_auxv(const uintptr_t info[]) {
  /* The layout of _start's argument is
   * info[0]: fini.
   * info[1]: envc
   * info[2]: argc
   * info[3]...[3+argc]: argv (NULL terminated)
   * info[3+argc+1]...[3+argc+1+envc]: envv (NULL terminated)
   * info[3+argc+1+envc+1]: auxv pairs.
   */
  int envc = info[1];
  int argc = info[2];
  return (Elf_auxv_t*) (info + 3 + envc + 1 + argc + 1);
}

static void grok_auxv(const Elf_auxv_t* auxv) {
  const Elf_auxv_t* av;
  for (av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO) {
      __nacl_irt_query = (TYPE_nacl_irt_query) av->a_un.a_val;
    }
  }
}

#define DO_QUERY(ident, name) __nacl_irt_query(ident, &name, sizeof(name))

static int my_strcmp(const char* a, const char* b) {
  while (*a == *b) {
    if (*a == '\0')
      return 0;
    ++a;
    ++b;
  }
  return (int) (unsigned char) *a - (int) (unsigned char) *b;
}

static PP_Bool DidCreate(PP_Instance instance,
                         uint32_t argc,
                         const char* argn[],
                         const char* argv[]) {
  return 1;
}

static void DidDestroy(PP_Instance instance) {
}

static void DidChangeView(PP_Instance instance,
                          const struct PP_Rect* position,
                          const struct PP_Rect* clip) {
}

static void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
}

static PP_Bool HandleDocumentLoad(PP_Instance instance,
                                  PP_Resource url_loader) {
  return 0;
}

static int32_t MyPPP_InitializeModule(PP_Module module_id,
                                      PPB_GetInterface get_browser_interface) {
  ppb_messaging = get_browser_interface(PPB_MESSAGING_INTERFACE_1_0);
  return PP_OK;
}

static void MyPPP_ShutdownModule(void) {
}

static const void* MyPPP_GetInterface(const char* interface_name) {
  if (my_strcmp(interface_name, PPP_INSTANCE_INTERFACE_1_0) == 0)
    return &ppp_instance;
  if (my_strcmp(interface_name, PPP_MESSAGING_INTERFACE_1_0) == 0)
    return &ppp_messaging;
  return NULL;
}

static void HandleMessage(PP_Instance instance, struct PP_Var message) {
  if (!ppb_messaging)
    __libnacl_irt_basic.exit(1);

  // Reply back.
  ppb_messaging->PostMessage(instance, message);
}

void _start(uintptr_t info[]) {
  Elf_auxv_t* auxv = nacl_startup_auxv(info);
  grok_auxv(auxv);
  DO_QUERY(NACL_IRT_BASIC_v0_1, __libnacl_irt_basic);

  struct nacl_irt_ppapihook ppapihook;
  DO_QUERY(NACL_IRT_PPAPIHOOK_v0_1, ppapihook);

  /* This is local as a workaround to avoid having to apply
   * relocations to global variables. */
  struct PP_StartFunctions start_funcs = {
    MyPPP_InitializeModule,
    MyPPP_ShutdownModule,
    MyPPP_GetInterface,
  };
  /* Similarly, initialise some global variables, avoiding relocations. */
  struct PPP_Instance_1_0 local_ppp_instance = {
    DidCreate,
    DidDestroy,
    DidChangeView,
    DidChangeFocus,
    HandleDocumentLoad,
  };
  ppp_instance = local_ppp_instance;
  struct PPP_Messaging_1_0 local_ppp_messaging = {
    HandleMessage,
  };
  ppp_messaging = local_ppp_messaging;

  ppapihook.ppapi_start(&start_funcs);

  __libnacl_irt_basic.exit(0);
}
