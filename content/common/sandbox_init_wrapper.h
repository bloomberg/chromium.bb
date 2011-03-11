// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_INIT_WRAPPER_H_
#define CONTENT_COMMON_SANDBOX_INIT_WRAPPER_H_
#pragma once

// Wraps the sandbox initialization and platform variables to consolodate
// the code and reduce the number of platform ifdefs elsewhere. The POSIX
// version of this wrapper is basically empty.

#include "build/build_config.h"

#include <string>

#include "base/basictypes.h"
#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

class CommandLine;

#if defined(OS_WIN)

class SandboxInitWrapper {
 public:
  SandboxInitWrapper() : broker_services_(), target_services_() { }
  // SetServices() needs to be called before InitializeSandbox() on Win32 with
  // the info received from the chrome exe main.
  void SetServices(sandbox::SandboxInterfaceInfo* sandbox_info);
  sandbox::BrokerServices* BrokerServices() const { return broker_services_; }
  sandbox::TargetServices* TargetServices() const { return target_services_; }

  // Initialize the sandbox for renderer, gpu, utility, worker, nacl, and
  // plug-in processes, depending on the command line flags. The browser
  // process is not sandboxed.
  // Returns true if the sandbox was initialized succesfully, false if an error
  // occurred.  If process_type isn't one that needs sandboxing true is always
  // returned.
  bool InitializeSandbox(const CommandLine& parsed_command_line,
                         const std::string& process_type);
 private:
  sandbox::BrokerServices* broker_services_;
  sandbox::TargetServices* target_services_;

  DISALLOW_COPY_AND_ASSIGN(SandboxInitWrapper);
};

#elif defined(OS_POSIX)

class SandboxInitWrapper {
 public:
  SandboxInitWrapper() { }

  // Initialize the sandbox for renderer and plug-in processes, depending on
  // the command line flags. The browser process is not sandboxed.
  // Returns true if the sandbox was initialized succesfully, false if an error
  // occurred.  If process_type isn't one that needs sandboxing true is always
  // returned.
  bool InitializeSandbox(const CommandLine& parsed_command_line,
                         const std::string& process_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxInitWrapper);
};

#endif

#endif  // CONTENT_COMMON_SANDBOX_INIT_WRAPPER_H_
