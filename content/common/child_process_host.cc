// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_host.h"

#include <limits>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "content/common/child_process_messages.h"
#include "content/public/common/child_process_host_delegate.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_logging.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#elif defined(OS_WIN)
#include "content/common/font_cache_dispatcher_win.h"
#endif  // OS_LINUX

#if defined(OS_MACOSX)
namespace {

// Given |path| identifying a Mac-style child process executable path, adjusts
// it to correspond to |feature|. For a child process path such as
// ".../Chromium Helper.app/Contents/MacOS/Chromium Helper", the transformed
// path for feature "NP" would be
// ".../Chromium Helper NP.app/Contents/MacOS/Chromium Helper NP". The new
// path is returned.
FilePath TransformPathForFeature(const FilePath& path,
                                 const std::string& feature) {
  std::string basename = path.BaseName().value();

  FilePath macos_path = path.DirName();
  const char kMacOSName[] = "MacOS";
  DCHECK_EQ(kMacOSName, macos_path.BaseName().value());

  FilePath contents_path = macos_path.DirName();
  const char kContentsName[] = "Contents";
  DCHECK_EQ(kContentsName, contents_path.BaseName().value());

  FilePath helper_app_path = contents_path.DirName();
  const char kAppExtension[] = ".app";
  std::string basename_app = basename;
  basename_app.append(kAppExtension);
  DCHECK_EQ(basename_app, helper_app_path.BaseName().value());

  FilePath root_path = helper_app_path.DirName();

  std::string new_basename = basename;
  new_basename.append(1, ' ');
  new_basename.append(feature);
  std::string new_basename_app = new_basename;
  new_basename_app.append(kAppExtension);

  FilePath new_path = root_path.Append(new_basename_app)
                               .Append(kContentsName)
                               .Append(kMacOSName)
                               .Append(new_basename);

  return new_path;
}

}  // namespace
#endif  // OS_MACOSX

ChildProcessHost::ChildProcessHost(content::ChildProcessHostDelegate* delegate)
    : delegate_(delegate),
      peer_handle_(base::kNullProcessHandle),
      opening_channel_(false) {
#if defined(OS_WIN)
  AddFilter(new FontCacheDispatcher());
#endif
}

ChildProcessHost::~ChildProcessHost() {
  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  base::CloseProcessHandle(peer_handle_);
}

void ChildProcessHost::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filters_.push_back(filter);

  if (channel_.get())
    filter->OnFilterAdded(channel_.get());
}

// static
FilePath ChildProcessHost::GetChildPath(int flags) {
  FilePath child_path;

  child_path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kBrowserSubprocessPath);

#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  // When running under Valgrind, forking /proc/self/exe ends up forking the
  // Valgrind executable, which then crashes. However, it's almost safe to
  // assume that the updates won't happen while testing with Valgrind tools.
  if (child_path.empty() && flags & CHILD_ALLOW_SELF && !RunningOnValgrind())
    child_path = FilePath("/proc/self/exe");
#endif

  // On most platforms, the child executable is the same as the current
  // executable.
  if (child_path.empty())
    PathService::Get(content::CHILD_PROCESS_EXE, &child_path);

#if defined(OS_MACOSX)
  DCHECK(!(flags & CHILD_NO_PIE && flags & CHILD_ALLOW_HEAP_EXECUTION));

  // If needed, choose an executable with special flags set that inform the
  // kernel to enable or disable specific optional process-wide features.
  if (flags & CHILD_NO_PIE) {
    // "NP" is "No PIE". This results in Chromium Helper NP.app or
    // Google Chrome Helper NP.app.
    child_path = TransformPathForFeature(child_path, "NP");
  } else if (flags & CHILD_ALLOW_HEAP_EXECUTION) {
    // "EH" is "Executable Heap". A non-executable heap is only available to
    // 32-bit processes on Mac OS X 10.7. Most code can and should run with a
    // non-executable heap, but the "EH" feature is provided to allow code
    // intolerant of a non-executable heap to work properly on 10.7. This
    // results in Chromium Helper EH.app or Google Chrome Helper EH.app.
    child_path = TransformPathForFeature(child_path, "EH");
  }
#endif

  return child_path;
}

void ChildProcessHost::ForceShutdown() {
  Send(new ChildProcessMsg_Shutdown());
}

bool ChildProcessHost::CreateChannel() {
  channel_id_ = GenerateRandomChannelID(this);
  channel_.reset(new IPC::Channel(
      channel_id_, IPC::Channel::MODE_SERVER, this));
  if (!channel_->Connect())
    return false;

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());

  // Make sure these messages get sent first.
#if defined(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::GetInstance()->Enabled();
  Send(new ChildProcessMsg_SetIPCLoggingEnabled(enabled));
#endif

  Send(new ChildProcessMsg_AskBeforeShutdown());

  opening_channel_ = true;

  return true;
}

bool ChildProcessHost::Send(IPC::Message* message) {
  if (!channel_.get()) {
    delete message;
    return false;
  }
  return channel_->Send(message);
}

void ChildProcessHost::AllocateSharedMemory(
      uint32 buffer_size, base::ProcessHandle child_process_handle,
      base::SharedMemoryHandle* shared_memory_handle) {
  base::SharedMemory shared_buf;
  if (!shared_buf.CreateAndMapAnonymous(buffer_size)) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map shared memory buffer";
    return;
  }
  shared_buf.GiveToProcess(child_process_handle, shared_memory_handle);
}

std::string ChildProcessHost::GenerateRandomChannelID(void* instance) {
  // Note: the string must start with the current process id, this is how
  // child processes determine the pid of the parent.
  // Build the channel ID.  This is composed of a unique identifier for the
  // parent browser process, an identifier for the child instance, and a random
  // component. We use a random component so that a hacked child process can't
  // cause denial of service by causing future named pipe creation to fail.
  return base::StringPrintf("%d.%p.%d",
                            base::GetCurrentProcId(), instance,
                            base::RandInt(0, std::numeric_limits<int>::max()));
}

int ChildProcessHost::GenerateChildProcessUniqueId() {
  // This function must be threadsafe.
  static base::subtle::Atomic32 last_unique_child_id = 0;
  return base::subtle::NoBarrier_AtomicIncrement(&last_unique_child_id, 1);
}

bool ChildProcessHost::OnMessageReceived(const IPC::Message& msg) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging* logger = IPC::Logging::GetInstance();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return true;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool handled = false;
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->OnMessageReceived(msg)) {
      handled = true;
      break;
    }
  }

  if (!handled) {
    handled = true;
    IPC_BEGIN_MESSAGE_MAP(ChildProcessHost, msg)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ShutdownRequest,
                          OnShutdownRequest)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_SyncAllocateSharedMemory,
                          OnAllocateSharedMemory)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    if (!handled)
      handled = delegate_->OnMessageReceived(msg);
  }

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg, channel_id_);
#endif
  return handled;
}

void ChildProcessHost::OnChannelConnected(int32 peer_pid) {
  if (!base::OpenProcessHandle(peer_pid, &peer_handle_)) {
    NOTREACHED();
  }
  opening_channel_ = false;
  delegate_->OnChannelConnected(peer_pid);
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelConnected(peer_pid);
}

void ChildProcessHost::OnChannelError() {
  opening_channel_ = false;
  delegate_->OnChannelError();

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // This will delete host_, which will also destroy this!
  delegate_->OnChildDisconnected();
}

void ChildProcessHost::OnAllocateSharedMemory(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  AllocateSharedMemory(buffer_size, peer_handle_, handle);
}

void ChildProcessHost::OnShutdownRequest() {
  if (delegate_->CanShutdown())
    Send(new ChildProcessMsg_Shutdown());
}
