/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <vector>
#include <map>

/*
 * NaCl Functions for intereacting with debuggers
 */

#include "native_client/src/debug_server/gdb_rsp/session.h"
#include "native_client/src/debug_server/gdb_rsp/target.h"
#include "native_client/src/debug_server/port/platform.h"
#include "native_client/src/debug_server/port/thread.h"

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/debug_server/debug_stub/debug_stub.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/debug_server/nacl_debug.h"

#include "native_client/src/trusted/service_runtime/sel_ldr.h"

using port::IPlatform;
using port::IThread;
using port::ITransport;

using gdb_rsp::Session;
using gdb_rsp::Target;

#ifdef WIN32
/* Disable warning for unwind disabled when exceptions used */
#pragma warning(disable:4530)
#endif

/*
 * These macro wraps all debugging stub calls to prevent C++ code called
 * by the debugging stub to throw and exception past the C API.  We use
 * this technique to allow the use of STL templates.   We catch bad_alloc
 * seperately purely to provide information for debugging purposes.
 */
#define DBG_CATCH_ALL                                                       \
  catch(std::bad_alloc) {                                                  \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Failed to allocate.\n", __LINE__); \
    exit(-1);                                                               \
  }                                                                         \
  catch(std::exception e) {                                                \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Caught exception: %s.\n",          \
            __LINE__ , e.what());                                           \
    exit(-1);                                                               \
  }                                                                         \
  catch(...) {                                                             \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Unexpected exception.\n", __LINE__);\
    exit(-1);                                                               \
  }


enum NaClDebugStatus {
  NDS_DISABLED = 0,
  NDS_ENABLED = 1,
  NDS_STOPPED = 2
};

/*
 * Remove name mangling to make it easier to find, incase we want to toggle
 * internal stub debugging from an external debugger.
 */
extern "C" {
  uint32_t nacl_debug_allowed = 0;
}

struct NaClDebugState {
  NaClDebugState() : target_(NULL), app_(NULL), break_(false),
                     errCode_(0), status_(NDS_DISABLED) {
#ifdef _DEBUG
    /*
     * When compiling DEBUG we allow an environment variable to enable
     * debugging, otherwise debugging could be allowed on a release
     * build by modifying nacl_debug_allowed.
     */
    if (NULL != getenv("NACL_DEBUG_ENABLE")) nacl_debug_allowed = 1;
#endif
    status_ = nacl_debug_allowed ? NDS_ENABLED : NDS_DISABLED;

    /* Initialize the stub if and only if debugging is enabled. */
    if (status_) {
      NaClDebugStubInit();
      target_ = new Target();

      /* If we failed to initialize the target, turn off debugging */
      if ((NULL == target_) || (!target_->Init())) status_ = NDS_DISABLED;
    }
  }

  ~NaClDebugState() {
    delete target_;
  }

  Target* target_;
  struct NaClApp *app_;
  bool break_;
  volatile int errCode_;
  NaClDebugStatus status_;
  nacl::string path_;
  std::vector<const char *> arg_;
  std::vector<const char *> env_;
};

/*
 * NOTE:  We use a singleton to delay construction allowing someone
 * to enable debugging only before the first use of this object.
 */
static NaClDebugState *NaClDebugGetState() {
  static NaClDebugState state;
  return &state;
}

void NaClDebugSetAllow(int enable) throw() {
#ifdef NACL_DEBUG_STUB
  nacl_debug_allowed = enable;
#else
  UNREFERENCED_PARAMETER(enable);
#endif
}

void NaClDebugSetStartBroken(int enable) throw() {
#ifdef NACL_DEBUG_STUB
  NaClDebugState* state = NaClDebugGetState();
  state->break_ = enable ? true : false;
#else
  UNREFERENCED_PARAMETER(enable);
#endif
}



void WINAPI NaClStubThread(void *ptr) {
#ifdef NACL_DEBUG_STUB
  uint32_t id = static_cast<uint32_t>(GetCurrentThreadId());
  printf("---> NaClStubThread(void *ptr)  thread id = %d == 0x%X\n", id, id);

  Target *targ = reinterpret_cast<Target*>(ptr);
  while (1) {
    ITransport* trans = NULL;
    Session* ses = NULL;

    try {
      // Wait for a connection.
      printf("---> %d\n", __LINE__); fflush(stdout);
      trans = ITransport::Accept("0.0.0.0:4014");
      printf("---> %d\n", __LINE__); fflush(stdout);
      if (NULL == trans) continue;
      printf("---> %d\n", __LINE__); fflush(stdout);

      // Create a new session for this connection
      ses = new Session();
      ses->Init(trans);
      ses->SetFlags(Session::DEBUG_MASK);

      // Run this session for as long as it lasts
      targ->Run(ses);
    }
    catch(...) {
      printf("---> %d\n", __LINE__); fflush(stdout);
      delete ses;
      printf("---> %d\n", __LINE__); fflush(stdout);
      ITransport::Free(trans);
      printf("---> %d\n", __LINE__); fflush(stdout);
    }
    printf("---> %d\n", __LINE__); fflush(stdout);
  }
#else
  UNREFERENCED_PARAMETER(ptr);
#endif
}

void NaClExceptionCatcher(uint32_t id, int8_t sig, void *cookie) {
#ifdef NACL_DEBUG_STUB
  Target* targ = static_cast<Target*>(cookie);

  /* Signal the target that we caught something */
  IPlatform::LogWarning("Caught signal %d on thread %Xh.\n", sig, id);
  targ->Signal(id, sig, true);
#else
  UNREFERENCED_PARAMETER(id);
  UNREFERENCED_PARAMETER(sig);
  UNREFERENCED_PARAMETER(cookie);
#endif
}


int NaClDebugIsEnabled(void) throw() {
#ifdef NACL_DEBUG_STUB
  try {
    return (NDS_ENABLED == NaClDebugGetState()->status_) ? 1 : 0;
  } DBG_CATCH_ALL
#endif

  return false;
}

void NaClDebugSetAppPath(const char *path) throw() {
  try {
    if (NaClDebugIsEnabled()) NaClDebugGetState()->path_ = path;
  } DBG_CATCH_ALL
}


void NaClDebugSetAppInfo(struct NaClApp *app) throw() {
#ifdef NACL_DEBUG_STUB
  if (NaClDebugIsEnabled()) {
    NaClDebugState *state = NaClDebugGetState();
    state->app_ = app;
  }
#else
  UNREFERENCED_PARAMETER(app);
#endif
}


void NaClDebugSetAppEnvironment(int argc, char const * const argv[],
                                int envc, char const * const envv[]) throw() {
  if (NaClDebugIsEnabled()) {
    int a;
    try {
      /*
       * Copy the pointer arrays.  We use ptrs instead of strings
       * since the data persits and it prevents an extra copy.
       */
      NaClDebugGetState()->arg_.resize(argc);
      for (a = 0; a < argc; a++) NaClDebugGetState()->arg_[a] = argv[a];
      NaClDebugGetState()->env_.resize(envc);
      for (a = 0; a < envc; a++) NaClDebugGetState()->env_[a] = envv[a];
    } DBG_CATCH_ALL
  }
}

void NaClDebugThreadPrepDebugging(struct NaClAppThread *natp) throw() {
  UNREFERENCED_PARAMETER(natp);

#ifdef NACL_DEBUG_STUB
  if (NaClDebugIsEnabled()) {
    NaClDebugState *state = NaClDebugGetState();
    uint32_t id = IPlatform::GetCurrentThread();
    IThread* thread = IThread::Acquire(id, true);
    state->target_->TrackThread(thread);

    /*
     * TODO(noelallen) We need to associate the natp with this thread
     * so we can get to the untrusted context preserved on a syscall.
     */
  }
#endif
}

void NaClDebugThreadStopDebugging(struct NaClAppThread *natp) throw() {
  UNREFERENCED_PARAMETER(natp);

#ifdef NACL_DEBUG_STUB
  if (NaClDebugIsEnabled()) {
    NaClDebugState *state = NaClDebugGetState();
    uint32_t id = IPlatform::GetCurrentThread();
    IThread* thread = IThread::Acquire(id, false);
    state->target_->IgnoreThread(thread);
    IThread::Release(thread);

    /*
     * TODO(noelallen) We need to associate the natp with this thread
     * so we can get to the thread once we support freeing a thread
     * from a different thread than the executing one.
     */
  }
#endif
}


int NaClDebugStart(void) throw() {
#ifdef NACL_DEBUG_STUB
  if (NaClDebugIsEnabled()) {
    NaClThread *thread = new NaClThread;

    if (NULL == thread) return false;
    NaClDebugState *state = NaClDebugGetState();

    /* If break on start has been requested, add a temp breakpoint. */
    if (state->break_) {
      struct NaClApp* app = state->app_;
      state->target_->AddTemporaryBreakpoint(app->entry_pt + app->mem_start);
    }

    NaClLog(LOG_WARNING, "nacl_debug(%d) : Debugging started.\n", __LINE__);
    IThread::SetExceptionCatch(NaClExceptionCatcher, state->target_);
    return NaClThreadCtor(thread, NaClStubThread, state->target_,
                          NACL_KERN_STACK_SIZE);
  }
#endif
  return 0;
}

void NaClDebugStop(int ErrCode) throw() {
#ifdef NACL_DEBUG_STUB

  /*
   * We check if debugging is enabled since this check is the only
   * mechanism for allocating the state object.  We free the
   * resources but not the object itself.  Instead we mark it as
   * STOPPED to prevent it from getting recreated.
   */
  if (NaClDebugIsEnabled()) {
    NaClDebugGetState()->status_ = NDS_STOPPED;
    NaClDebugGetState()->errCode_ = ErrCode;
  }
#else
  UNREFERENCED_PARAMETER(ErrCode);
#endif
}

