// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_host.h"

#include "base/command_line.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif  // OS_LINUX

ChildProcessHost::ChildProcessHost()
    : ALLOW_THIS_IN_INITIALIZER_LIST(listener_(this)),
      opening_channel_(false) {
}

ChildProcessHost::~ChildProcessHost() {
}

// static
FilePath ChildProcessHost::GetChildPath(bool allow_self) {
  FilePath child_path;

  child_path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kBrowserSubprocessPath);
  if (!child_path.empty())
    return child_path;

#if defined(OS_MACOSX)
  // On the Mac, the child executable lives at a predefined location within
  // the app bundle's versioned directory.
  return chrome::GetVersionedDirectory().
      Append(chrome::kHelperProcessExecutablePath);
#endif

#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  if (allow_self)
    return FilePath("/proc/self/exe");
#endif

  // On most platforms, the child executable is the same as the current
  // executable.
  PathService::Get(base::FILE_EXE, &child_path);
  return child_path;
}

#if defined(OS_WIN)
// static
void ChildProcessHost::PreCacheFont(LOGFONT font) {
  // If a child process is running in a sandbox, GetTextMetrics()
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics() will try to load the font
  // from the font file. However, the sandboxed process does
  // not have permissions to access any font files and
  // the call fails. So we make the browser pre-load the
  // font for us by using a dummy call to GetTextMetrics of
  // the same font.

  // Maintain a circular queue for the fonts and DCs to be cached.
  // font_index maintains next available location in the queue.
  static const int kFontCacheSize = 32;
  static HFONT fonts[kFontCacheSize] = {0};
  static HDC hdcs[kFontCacheSize] = {0};
  static size_t font_index = 0;

  UMA_HISTOGRAM_COUNTS_100("Memory.CachedFontAndDC",
      fonts[kFontCacheSize-1] ? kFontCacheSize : static_cast<int>(font_index));

  HDC hdc = GetDC(NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  TEXTMETRIC tm;
  BOOL ret = GetTextMetrics(hdc, &tm);
  DCHECK(ret);

  if (fonts[font_index] || hdcs[font_index]) {
    // We already have too many fonts, we will delete one and take it's place.
    DeleteObject(fonts[font_index]);
    ReleaseDC(NULL, hdcs[font_index]);
  }

  fonts[font_index] = font_handle;
  hdcs[font_index] = hdc;
  font_index = (font_index + 1) % kFontCacheSize;
}
#endif  // OS_WIN


bool ChildProcessHost::CreateChannel() {
  channel_id_ = ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(new IPC::Channel(
      channel_id_, IPC::Channel::MODE_SERVER, &listener_));
  if (!channel_->Connect())
    return false;

  // Make sure these messages get sent first.
#if defined(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::current()->Enabled();
  SendOnChannel(new PluginProcessMsg_SetIPCLoggingEnabled(enabled));
#endif

  SendOnChannel(new PluginProcessMsg_AskBeforeShutdown());

  opening_channel_ = true;

  return true;
}

void ChildProcessHost::InstanceCreated() {
  Notify(NotificationType::CHILD_INSTANCE_CREATED);
}

bool ChildProcessHost::SendOnChannel(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

void ChildProcessHost::OnChildDied() {
  delete this;
}

ChildProcessHost::ListenerHook::ListenerHook(ChildProcessHost* host)
    : host_(host) {
}

void ChildProcessHost::ListenerHook::OnMessageReceived(
    const IPC::Message& msg) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging* logger = IPC::Logging::current();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool handled = host_->InterceptMessageFromChild(msg);

  if (!handled) {
    if (msg.type() == PluginProcessHostMsg_ShutdownRequest::ID) {
      if (host_->CanShutdown())
        host_->SendOnChannel(new PluginProcessMsg_Shutdown());
    } else {
      host_->OnMessageReceived(msg);
    }
  }

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg, host_->channel_id_);
#endif
}

void ChildProcessHost::ListenerHook::OnChannelConnected(int32 peer_pid) {
  host_->opening_channel_ = false;
  host_->OnChannelConnected(peer_pid);
  // Notify in the main loop of the connection.
  host_->Notify(NotificationType::CHILD_PROCESS_HOST_CONNECTED);
}

void ChildProcessHost::ListenerHook::OnChannelError() {
  host_->opening_channel_ = false;
  host_->OnChannelError();

  // This will delete host_, which will also destroy this!
  host_->OnChildDied();
}

void ChildProcessHost::ForceShutdown() {
  SendOnChannel(new PluginProcessMsg_Shutdown());
}
