// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_TRANSLATE_THREAD_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_TRANSLATE_THREAD_H_

#include <deque>
#include <vector>

#include "components/nacl/renderer/plugin/plugin_error.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "ppapi/cpp/completion_callback.h"

struct PP_PNaClOptions;

namespace nacl {
class DescWrapper;
}


namespace plugin {

class NaClSubprocess;
class PnaclCoordinator;
class TempFile;

class PnaclTranslateThread {
 public:
  PnaclTranslateThread();
  ~PnaclTranslateThread();

  // Set up the state for RunCompile and RunLink. When an error is
  // encountered, or RunLink is complete the finish_callback is run
  // to notify the main thread.
  void SetupState(const pp::CompletionCallback& finish_callback,
                  NaClSubprocess* compiler_subprocess,
                  NaClSubprocess* ld_subprocess,
                  const std::vector<TempFile*>* obj_files,
                  int num_threads,
                  TempFile* nexe_file,
                  nacl::DescWrapper* invalid_desc_wrapper,
                  ErrorInfo* error_info,
                  PP_PNaClOptions* pnacl_options,
                  const std::string& architecture_attributes,
                  PnaclCoordinator* coordinator);

  // Create a compile thread and run/command the compiler_subprocess.
  // It will continue to run and consume data as it is passed in with PutBytes.
  // On success, runs compile_finished_callback.
  // On error, runs finish_callback.
  // The compiler_subprocess must already be loaded.
  void RunCompile(const pp::CompletionCallback& compile_finished_callback);

  // Create a link thread and run/command the ld_subprocess.
  // On completion (success or error), runs finish_callback.
  // The ld_subprocess must already be loaded.
  void RunLink();

  // Kill the llc and/or ld subprocesses. This happens by closing the command
  // channel on the plugin side, which causes the trusted code in the nexe to
  // exit, which will cause any pending SRPCs to error. Because this is called
  // on the main thread, the translation thread must not use the subprocess
  // objects without the lock, other than InvokeSrpcMethod, which does not
  // race with service runtime shutdown.
  void AbortSubprocesses();

  // Send bitcode bytes to the translator. Called from the main thread.
  void PutBytes(const void* data, int count);

  // Notify the translator that the end of the bitcode stream has been reached.
  // Called from the main thread.
  void EndStream();

  int64_t GetCompileTime() const { return compile_time_; }

  // Returns true if the translation process is initiated via SetupState.
  bool started() const { return coordinator_ != NULL; }

 private:
  // Helper thread entry point for compilation. Takes a pointer to
  // PnaclTranslateThread and calls DoCompile().
  static void WINAPI DoCompileThread(void* arg);
  // Runs the streaming compilation. Called from the helper thread.
  void DoCompile();

  // Similar to DoCompile*, but for linking.
  static void WINAPI DoLinkThread(void* arg);
  void DoLink();

  // Signal that Pnacl translation failed, from the translation thread only.
  void TranslateFailed(PP_NaClError err_code,
                       const std::string& error_string);

  // Callback to run when compile is completed and linking can start.
  pp::CompletionCallback compile_finished_callback_;

  // Callback to run when tasks are completed or an error has occurred.
  pp::CompletionCallback report_translate_finished_;

  nacl::scoped_ptr<NaClThread> translate_thread_;

  // Used to guard compiler_subprocess, ld_subprocess,
  // compiler_subprocess_active_, and ld_subprocess_active_
  // (touched by the main thread and the translate thread).
  struct NaClMutex subprocess_mu_;
  // The compiler_subprocess and ld_subprocess memory is owned by the
  // coordinator so we do not delete them. However, the main thread delegates
  // shutdown to this thread, since this thread may still be accessing the
  // subprocesses. The *_subprocess_active flags indicate which subprocesses
  // are active to ensure the subprocesses don't get shutdown more than once.
  // The subprocess_mu_ must be held when shutting down the subprocesses
  // or otherwise accessing the service_runtime component of the subprocess.
  // There are some accesses to the subprocesses without locks held
  // (invoking srpc_client methods -- in contrast to using the service_runtime).
  NaClSubprocess* compiler_subprocess_;
  NaClSubprocess* ld_subprocess_;
  bool compiler_subprocess_active_;
  bool ld_subprocess_active_;

  // Condition variable to synchronize communication with the SRPC thread.
  // SRPC thread waits on this condvar if data_buffers_ is empty (meaning
  // there is no bitcode to send to the translator), and the main thread
  // appends to data_buffers_ and signals it when it receives bitcode.
  struct NaClCondVar buffer_cond_;
  // Mutex for buffer_cond_.
  struct NaClMutex cond_mu_;
  // Data buffers from FileDownloader are enqueued here to pass from the
  // main thread to the SRPC thread. Protected by cond_mu_
  std::deque<std::vector<char> > data_buffers_;
  // Whether all data has been downloaded and copied to translation thread.
  // Associated with buffer_cond_
  bool done_;

  int64_t compile_time_;

  // Data about the translation files, owned by the coordinator
  const std::vector<TempFile*>* obj_files_;
  int num_threads_;
  TempFile* nexe_file_;
  nacl::DescWrapper* invalid_desc_wrapper_;
  ErrorInfo* coordinator_error_info_;
  PP_PNaClOptions* pnacl_options_;
  std::string architecture_attributes_;
  PnaclCoordinator* coordinator_;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclTranslateThread);
};

}
#endif // COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_TRANSLATE_THREAD_H_
