/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef __GLIBC__
#include <elf.h>
#include <link.h>
#endif  /* __GLIBC__ */

#include "untrusted_crash_dump.h"
#include "string_stream.h"
#include "irt.h"

struct NaClExceptionContext {
  uint32_t prog_ctr;
  uint32_t stack_ptr;
  uint32_t frame_ptr;
  /*
   * Pad this up to an even multiple of 8 bytes so this struct will add
   * a predictable amount of space to the various ExceptionFrame structures
   * used on each platform. This allows us to verify stack layout with dead
   * reckoning, without access to the ExceptionFrame structure used to set up
   * the call stack.
   */
  uint32_t pad;
};

#define CRASH_PAGE_CHUNK (64 * 1024)
#define CRASH_STACK_SIZE (CRASH_PAGE_CHUNK * 4)
#define CRASH_STACK_GUARD_SIZE CRASH_PAGE_CHUNK
#define CRASH_STACK_COMPLETE_SIZE (CRASH_STACK_GUARD_SIZE + CRASH_STACK_SIZE)


static pthread_key_t g_CrashStackKey;
static struct nacl_irt_dev_exception_handling g_ExceptionHandling;
static int g_ExceptionHandlingEnabled = 0;


#ifdef __GLIBC__

struct ProgramTableData {
  sstream_t *core;
  int first;
};

static void WriteJsonString(const char *str, sstream_t *core) {
  char ch;

  ssprintf(core, "\"");
  for (;;) {
    ch = *str++;
    if (ch == '\0') {
      break;
    } else if (ch == '"') {
      ssprintf(core, "\\\"");
    } else if (ch == '\\') {
      ssprintf(core, "\\\\");
    } else if (ch < 32 || ch > 126) {
      ssprintf(core, "\\x%02x", (uint8_t)ch);
    } else {
      ssprintf(core, "%c", ch);
    }
  }
  ssprintf(core, "\"");
}

static int PrintSegmentsOne(
    struct dl_phdr_info *info, size_t size, void *data) {
  int i;
  struct ProgramTableData *ptd = (struct ProgramTableData*) data;

  if (ptd->first) {
    ptd->first = 0;
  } else {
    fprintf(ptd->core, ",\n");
  }
  fprintf(ptd->core, "{\n");
  fprintf(ptd->core, "\"dlpi_name\": ");
  WriteJsonString(info->dlpi_name, ptd->core);
  fprintf(ptd->core, ",\n");
  fprintf(ptd->core, "\"dlpi_addr\": %u,\n",
          (uintptr_t)  info->dlpi_addr);
  fprintf(ptd->core, "\"dlpi_phdr\": [\n");
  for (i = 0; i < info->dlpi_phnum; i++) {
    /* Skip non-LOAD type segments. */
    if (info->dlpi_phdr[i].p_type != PT_LOAD) {
      continue;
    }
    if (i != 0) {
      fprintf(ptd->core, ",\n");
    }
    fprintf(ptd->core, "{\n");
    fprintf(ptd->core, "\"p_vaddr\": %u,\n",
            (uintptr_t) info->dlpi_phdr[i].p_vaddr);
    fprintf(ptd->core, "\"p_memsz\": %u\n",
            (uintptr_t) info->dlpi_phdr[i].p_memsz);
    fprintf(ptd->core, "}\n");
  }
  fprintf(ptd->core, "]\n");
  fprintf(ptd->core, "}\n");
  return 0;
}

static void PrintSegments(sstream_t *core) {
  struct ProgramTableData data;
  data.core = core;
  data.first = 1;
  dl_iterate_phdr(PrintSegmentsOne, &data);
}

#else  /* __GLIBC__ */

static void PrintSegments(sstream_t *core) {
}

#endif  /* __GLIBC__ */


void PostMessage(const char *str);


uintptr_t SafeRead(uintptr_t a) {
  /* TODO(bradnelson): use exception handling to recover from reads. */
  return *(uintptr_t*)a;
}

static void StackWalk(sstream_t *core, struct NaClExceptionContext *context) {
  uintptr_t next;
  uintptr_t i;
  int first = 1;
  uintptr_t prog_ctr = context->prog_ctr;
  uintptr_t frame_ptr = context->frame_ptr;
  uintptr_t args_start;

  ssprintf(core, "\"frames\": [\n");
  for (;;) {
    next = SafeRead(frame_ptr);
    if (next <= frame_ptr || next == 0) {
      break;
    }
    if (first) {
      first = 0;
    } else {
      ssprintf(core, ",");
    }
    ssprintf(core, "{\n");
    ssprintf(core, "\"frame_ptr\": %u,\n", frame_ptr);
    ssprintf(core, "\"prog_ctr\": %u,\n", prog_ctr);
    ssprintf(core, "\"data\": [\n");
#if defined(__x86_64__)
    args_start = frame_ptr + 8;
#else
    args_start = frame_ptr + 16;
#endif
    for (i = args_start; i < next && i - args_start < 100; i += 4) {
      if (i != args_start) {
        ssprintf(core, ",");
      }
      ssprintf(core, "%u\n", SafeRead(i));
    }
    ssprintf(core, "]\n");
    ssprintf(core, "}\n");
    
    if (next - frame_ptr > 10000) break;
#if defined(__x86_64__)
    prog_ctr = SafeRead(frame_ptr + 8);
#else
    prog_ctr = SafeRead(frame_ptr + 4);
#endif
    frame_ptr = next;
  }

  ssprintf(core, "]\n");
}

void CrashHandler(struct NaClExceptionContext *context) {
  sstream_t ss;
  FILE* handle = fopen("naclcorejson", "wb");

  ssinit(&ss);
  ssprintf(&ss, "TRC: {\n");

  ssprintf(&ss, "\"segments\": [");
  PrintSegments(&ss);
  ssprintf(&ss, "],\n");

  ssprintf(&ss, "\"handler\": {\n");
  ssprintf(&ss, "\"prog_ctr\": %u,\n", context->prog_ctr);
  ssprintf(&ss, "\"stack_ptr\": %u,\n", context->stack_ptr);
  ssprintf(&ss, "\"frame_ptr\": %u\n", context->frame_ptr);
  ssprintf(&ss, "},\n");

  StackWalk(&ss, context);

  ssprintf(&ss, "}\n");

  if (handle != NULL) {
    fprintf(handle, "%s", &ss.data[5]);
    fclose(handle);
  }

  PostMessage(ss.data);

  while(1);
}

void NaClCrashDumpThreadDestructor(void *arg) {
  munmap(arg, CRASH_STACK_COMPLETE_SIZE);
}

int NaClCrashDumpInit(void) {
  int result;

  assert(g_ExceptionHandlingEnabled == 0);
  if (nacl_interface_query(NACL_IRT_DEV_EXCEPTION_HANDLING_v0_1,
                           &g_ExceptionHandling,
                           sizeof(g_ExceptionHandling)) == 0) {
    fprintf(stderr, "ERROR: failed nacl_interface_query\n");
    return 0;
  }
  result = pthread_key_create(&g_CrashStackKey, NaClCrashDumpThreadDestructor);
  assert(result == 0);
  if (g_ExceptionHandling.exception_handler(CrashHandler, NULL) != 0) {
    fprintf(stderr, "ERROR: failed exception_handler\n");
    return 0;
  }
  g_ExceptionHandlingEnabled = 1;
  if (!NaClCrashDumpInitThread()) {
    g_ExceptionHandlingEnabled = 0;
    fprintf(stderr, "ERROR: failed InitThread\n");
    return 0;
  }
  return 1;
}

int NaClCrashDumpInitThread(void) {
  void *stack;
  void *guard;
  int result;

  if (!g_ExceptionHandlingEnabled) {
    return 0;
  }
  /*
   * NOTE: Setting up a per thread stack is only particularly interesting
   *       for stack overflow.
   */
  stack = mmap(NULL, CRASH_STACK_COMPLETE_SIZE,
               PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(stack != MAP_FAILED);
  guard = mmap(stack, CRASH_STACK_GUARD_SIZE,
               PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(guard == stack);
  pthread_setspecific(g_CrashStackKey, stack);
  result = g_ExceptionHandling.exception_stack(
      stack, CRASH_STACK_COMPLETE_SIZE);
  return result == 0;
}
