// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_proxy/process_proxy_registry.h"

#include "base/bind.h"

namespace chromeos {

namespace {

const char kWatcherThreadName[] = "ProcessWatcherThread";

const char kStdoutOutputType[] = "stdout";
const char kExitOutputType[] = "exit";

const char* ProcessOutputTypeToString(ProcessOutputType type) {
  switch (type) {
    case PROCESS_OUTPUT_TYPE_OUT:
      return kStdoutOutputType;
    case PROCESS_OUTPUT_TYPE_EXIT:
      return kExitOutputType;
    default:
      return NULL;
  }
}

static base::LazyInstance<ProcessProxyRegistry> g_process_proxy_registry =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ProcessProxyRegistry::ProcessProxyInfo::ProcessProxyInfo() {
}

ProcessProxyRegistry::ProcessProxyInfo::ProcessProxyInfo(
    const ProcessProxyInfo& other) {
  // This should be called with empty info only.
  DCHECK(!other.proxy.get());
}

ProcessProxyRegistry::ProcessProxyInfo::~ProcessProxyInfo() {
}

ProcessProxyRegistry::ProcessProxyRegistry() {
}

ProcessProxyRegistry::~ProcessProxyRegistry() {
  // TODO(tbarzic): Fix issue with ProcessProxyRegistry being destroyed
  // on a different thread (it's a LazyInstance).
  DetachFromThread();

  ShutDown();
}

void ProcessProxyRegistry::ShutDown() {
  // Close all proxies we own.
  while (!proxy_map_.empty())
    CloseProcess(proxy_map_.begin()->first);

  if (watcher_thread_) {
    watcher_thread_->Stop();
    watcher_thread_.reset();
  }
}

// static
ProcessProxyRegistry* ProcessProxyRegistry::Get() {
  return g_process_proxy_registry.Pointer();
}

bool ProcessProxyRegistry::OpenProcess(
    const std::string& command,
    pid_t* pid,
    const ProcessOutputCallbackWithPid& callback) {
  DCHECK(CalledOnValidThread());

  if (!EnsureWatcherThreadStarted())
    return false;

  // Create and open new proxy.
  scoped_refptr<ProcessProxy> proxy(new ProcessProxy());
  if (!proxy->Open(command, pid))
    return false;

  // Kick off watcher.
  // We can use Unretained because proxy will stop calling callback after it is
  // closed, which is done before this object goes away.
  if (!proxy->StartWatchingOutput(
          watcher_thread_->task_runner(),
          base::Bind(&ProcessProxyRegistry::OnProcessOutput,
                     base::Unretained(this), *pid))) {
    proxy->Close();
    return false;
  }

  DCHECK(proxy_map_.find(*pid) == proxy_map_.end());

  // Save info for newly created proxy. We cannot do this before ProcessProxy is
  // created because we don't know |pid| then.
  ProcessProxyInfo& info = proxy_map_[*pid];
  info.proxy.swap(proxy);
  info.process_id = *pid;
  info.callback = callback;
  return true;
}

bool ProcessProxyRegistry::SendInput(pid_t pid, const std::string& data) {
  DCHECK(CalledOnValidThread());

  std::map<pid_t, ProcessProxyInfo>::iterator it = proxy_map_.find(pid);
  if (it == proxy_map_.end())
    return false;
  return it->second.proxy->Write(data);
}

bool ProcessProxyRegistry::CloseProcess(pid_t pid) {
  DCHECK(CalledOnValidThread());

  std::map<pid_t, ProcessProxyInfo>::iterator it = proxy_map_.find(pid);
  if (it == proxy_map_.end())
    return false;

  it->second.proxy->Close();
  proxy_map_.erase(it);
  return true;
}

bool ProcessProxyRegistry::OnTerminalResize(pid_t pid, int width, int height) {
  DCHECK(CalledOnValidThread());

  std::map<pid_t, ProcessProxyInfo>::iterator it = proxy_map_.find(pid);
  if (it == proxy_map_.end())
    return false;

  return it->second.proxy->OnTerminalResize(width, height);
}

void ProcessProxyRegistry::OnProcessOutput(pid_t pid,
    ProcessOutputType type, const std::string& data) {
  DCHECK(CalledOnValidThread());

  const char* type_str = ProcessOutputTypeToString(type);
  DCHECK(type_str);

  std::map<pid_t, ProcessProxyInfo>::iterator it = proxy_map_.find(pid);
  if (it == proxy_map_.end())
    return;
  it->second.callback.Run(pid, std::string(type_str), data);

  // Contact with the slave end of the terminal has been lost. We have to close
  // the process.
  if (type == PROCESS_OUTPUT_TYPE_EXIT)
    CloseProcess(pid);
}

bool ProcessProxyRegistry::EnsureWatcherThreadStarted() {
  if (watcher_thread_.get())
    return true;

  // TODO(tbarzic): Change process output watcher to watch for fd readability on
  //    FILE thread, and move output reading to worker thread instead of
  //    spinning a new thread.
  watcher_thread_.reset(new base::Thread(kWatcherThreadName));
  return watcher_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
}

}  // namespace chromeos
