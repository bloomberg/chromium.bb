// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_INFO_H_
#define CONTENT_COMMON_CHILD_PROCESS_INFO_H_
#pragma once

#include <string>

#include "base/process.h"
#include "base/string16.h"
#include "content/common/content_export.h"

// Holds information about a child process.
class CONTENT_EXPORT ChildProcessInfo {
 public:
  // NOTE: Do not remove or reorder the elements in this enum, and only add new
  // items at the end, right before MAX_PROCESS. We depend on these specific
  // values in histograms.
  enum ProcessType {
    UNKNOWN_PROCESS = 1,
    BROWSER_PROCESS,
    RENDER_PROCESS,
    PLUGIN_PROCESS,
    WORKER_PROCESS,
    NACL_LOADER_PROCESS,
    UTILITY_PROCESS,
    PROFILE_IMPORT_PROCESS,
    ZYGOTE_PROCESS,
    SANDBOX_HELPER_PROCESS,
    NACL_BROKER_PROCESS,
    GPU_PROCESS,
    PPAPI_PLUGIN_PROCESS,
    PPAPI_BROKER_PROCESS,
    MAX_PROCESS
  };

  ChildProcessInfo(const ChildProcessInfo& original);
  virtual ~ChildProcessInfo();

  ChildProcessInfo& operator=(const ChildProcessInfo& original);

  // Returns the type of the process.
  ProcessType type() const { return type_; }

  // Returns the name of the process.  i.e. for plugins it might be Flash, while
  // for workers it might be the domain that it's from.
  const string16& name() const { return name_; }

  // Returns the version of the exe, this only applies to plugins. Otherwise
  // the string is empty.
  const string16& version() const { return version_; }

  // Getter to the process handle.
  base::ProcessHandle handle() const { return process_.handle(); }

  // Getter to the process ID.
  int pid() const { return process_.pid(); }

  // The unique identifier for this child process. This identifier is NOT a
  // process ID, and will be unique for all types of child process for
  // one run of the browser.
  int id() const { return id_; }

  void SetProcessBackgrounded() const { process_.SetProcessBackgrounded(true); }

  // Returns an English name of the process type, should only be used for non
  // user-visible strings, or debugging pages like about:memory.
  static std::string GetTypeNameInEnglish(ProcessType type);

  // We define the < operator so that the ChildProcessInfo can be used as a key
  // in a std::map.
  bool operator <(const ChildProcessInfo& rhs) const {
    if (process_.handle() != rhs.process_.handle())
      return process_ .handle() < rhs.process_.handle();
    return false;
  }

  bool operator ==(const ChildProcessInfo& rhs) const {
    return process_.handle() == rhs.process_.handle();
  }

  // Generates a unique channel name for a child renderer/plugin process.
  // The "instance" pointer value is baked into the channel id.
  static std::string GenerateRandomChannelID(void* instance);

  // Returns a unique ID to identify a child process. On construction, this
  // function will be used to generate the id_, but it is also used to generate
  // IDs for the RenderProcessHost, which doesn't inherit from us, and whose IDs
  // must be unique for all child processes.
  //
  // This function is threadsafe since RenderProcessHost is on the UI thread,
  // but normally this will be used on the IO thread.
  static int GenerateChildProcessUniqueId();

 protected:
  // Derived objects need to use this constructor so we know what type we are.
  // If the caller has already generated a unique ID for this child process,
  // it should pass it as the second argument. Otherwise, -1 should be passed
  // and a unique ID will be automatically generated.
  ChildProcessInfo(ProcessType type, int id);

  void set_type(ProcessType type) { type_ = type; }
  void set_name(const string16& name) { name_ = name; }
  void set_version(const string16& ver) { version_ = ver; }
  void set_handle(base::ProcessHandle handle) { process_.set_handle(handle); }

 private:
  ProcessType type_;
  string16 name_;
  string16 version_;
  int id_;

  // The handle to the process.
  mutable base::Process process_;
};

#endif  // CONTENT_COMMON_CHILD_PROCESS_INFO_H_
