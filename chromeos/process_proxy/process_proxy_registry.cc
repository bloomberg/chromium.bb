// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_proxy/process_proxy_registry.h"

#include "base/bind.h"

namespace chromeos {

namespace {

const char kWatcherThreadName[] = "ProcessWatcherThread";

const char kStdoutOutputType[] = "stdout";
const char kStderrOutputType[] = "stderr";
const char kExitOutputType[] = "exit";

const char* ProcessOutputTypeToString(ProcessOutputType type) {
  switch (type) {
    case PROCESS_OUTPUT_TYPE_OUT:
      return kStdoutOutputType;
    case PROCESS_OUTPUT_TYPE_ERR:
      return kStderrOutputType;
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
  DCHECK(!other.proxy.get() && !other.watcher_thread.get());
}

ProcessProxyRegistry::ProcessProxyInfo::~ProcessProxyInfo() {
}

ProcessProxyRegistry::ProcessProxyRegistry() {
}

ProcessProxyRegistry::~ProcessProxyRegistry() {
  // TODO(tbarzic): Fix issue with ProcessProxyRegistry being destroyed
  // on a different thread (it's a LazyInstance).
  DetachFromThread();

  // Close all proxies we own.
  while (!proxy_map_.empty())
    CloseProcess(proxy_map_.begin()->first);
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

  // TODO(tbarzic): Instead of creating a new thread for each new process proxy,
  // use one thread for all processes.
  // We will need new thread for proxy's outpu watcher.
  scoped_ptr<base::Thread> watcher_thread(new base::Thread(kWatcherThreadName));
  if (!watcher_thread->Start()) {
    return false;
  }

  // Create and open new proxy.
  scoped_refptr<ProcessProxy> proxy(new ProcessProxy());
  if (!proxy->Open(command, pid))
    return false;

  // Kick off watcher.
  // We can use Unretained because proxy will stop calling callback after it is
  // closed, which is done befire this object goes away.
  if (!proxy->StartWatchingOnThread(watcher_thread.get(),
           base::Bind(&ProcessProxyRegistry::OnProcessOutput,
                      base::Unretained(this), *pid))) {
    proxy->Close();
    watcher_thread->Stop();
    return false;
  }

  DCHECK(proxy_map_.find(*pid) == proxy_map_.end());

  // Save info for newly created proxy. We cannot do this before ProcessProxy is
  // created because we don't know |pid| then.
  ProcessProxyInfo& info = proxy_map_[*pid];
  info.proxy.swap(proxy);
  info.watcher_thread.reset(watcher_thread.release());
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
  it->second.watcher_thread->Stop();
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

}  // namespace chromeos
