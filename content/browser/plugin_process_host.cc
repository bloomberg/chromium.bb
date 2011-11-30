// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_process_host.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include <windows.h>
#elif defined(OS_POSIX)
#include <utility>  // for pair<>
#endif

#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/plugin_service.h"
#include "content/common/plugin_messages.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/gl/gl_switches.h"
#include "ui/gfx/native_widget_types.h"

using content::BrowserThread;

#if defined(USE_X11)
#include "ui/gfx/gtk_native_view_id_manager.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "content/common/plugin_carbon_interpose_constants_mac.h"
#include "ui/gfx/rect.h"
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
#include "base/win/windows_version.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

namespace {

void ReparentPluginWindowHelper(HWND window, HWND parent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int window_style = WS_CHILD;
  if (!webkit::npapi::WebPluginDelegateImpl::IsDummyActivationWindow(window))
    window_style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

  ::SetWindowLongPtr(window, GWL_STYLE, window_style);
  ::SetParent(window, parent);
  // Allow the Flash plugin to forward some messages back to Chrome.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    ::SetPropW(parent, webkit::npapi::kNativeWindowClassFilterProp, HANDLE(-1));
}

}  // namespace

void PluginProcessHost::OnPluginWindowDestroyed(HWND window, HWND parent) {
  // The window is destroyed at this point, we just care about its parent, which
  // is the intermediate window we created.
  std::set<HWND>::iterator window_index =
      plugin_parent_windows_set_.find(parent);
  if (window_index == plugin_parent_windows_set_.end())
    return;

  plugin_parent_windows_set_.erase(window_index);
  PostMessage(parent, WM_CLOSE, 0, 0);
}

void PluginProcessHost::AddWindow(HWND window) {
  plugin_parent_windows_set_.insert(window);
}

void PluginProcessHost::OnReparentPluginWindow(HWND window, HWND parent) {
  // Reparent only from the plugin process to our process.
  DWORD process_id = 0;
  ::GetWindowThreadProcessId(window, &process_id);
  if (process_id != ::GetProcessId(GetChildProcessHandle()))
    return;
  ::GetWindowThreadProcessId(parent, &process_id);
  if (process_id != ::GetCurrentProcessId())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(ReparentPluginWindowHelper, window, parent));
}
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_USES_GTK)
void PluginProcessHost::OnMapNativeViewId(gfx::NativeViewId id,
                                          gfx::PluginWindowHandle* output) {
  *output = 0;
#if !defined(USE_AURA)
  GtkNativeViewManager::GetInstance()->GetXIDForId(output, id);
#endif
}
#endif  // defined(TOOLKIT_USES_GTK)

PluginProcessHost::PluginProcessHost()
    : BrowserChildProcessHost(PLUGIN_PROCESS)
#if defined(OS_MACOSX)
      , plugin_cursor_visible_(true)
#endif
{
}

PluginProcessHost::~PluginProcessHost() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // We erase HWNDs from the plugin_parent_windows_set_ when we receive a
  // notification that the window is being destroyed. If we don't receive this
  // notification and the PluginProcessHost instance is being destroyed, it
  // means that the plugin process crashed. We paint a sad face in this case in
  // the renderer process. To ensure that the sad face shows up, and we don't
  // leak HWNDs, we should destroy existing plugin parent windows.
  std::set<HWND>::iterator window_index;
  for (window_index = plugin_parent_windows_set_.begin();
       window_index != plugin_parent_windows_set_.end();
       window_index++) {
    PostMessage(*window_index, WM_CLOSE, 0, 0);
  }
#elif defined(OS_MACOSX)
  // If the plugin process crashed but had fullscreen windows open at the time,
  // make sure that the menu bar is visible.
  std::set<uint32>::iterator window_index;
  for (window_index = plugin_fullscreen_windows_set_.begin();
       window_index != plugin_fullscreen_windows_set_.end();
       window_index++) {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::mac::ReleaseFullScreen(base::mac::kFullScreenModeHideAll);
    } else {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(base::mac::ReleaseFullScreen,
                                         base::mac::kFullScreenModeHideAll));
    }
  }
  // If the plugin hid the cursor, reset that.
  if (!plugin_cursor_visible_) {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::mac::SetCursorVisibility(true);
    } else {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(base::mac::SetCursorVisibility, true));
    }
  }
#endif
  // Cancel all pending and sent requests.
  CancelRequests();
}

bool PluginProcessHost::Init(const webkit::WebPluginInfo& info,
                             const std::string& locale) {
  info_ = info;
  set_name(info_.name);
  set_version(info_.version);

  if (!CreateChannel())
    return false;

  // Build command line for plugin. When we have a plugin launcher, we can't
  // allow "self" on linux and we need the real file path.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPluginLauncher);

#if defined(OS_MACOSX)
  // Run the plug-in process in a mode tolerant of heap execution without
  // explicit mprotect calls. Some plug-ins still rely on this quaint and
  // archaic "feature." See http://crbug.com/93551.
  int flags = CHILD_ALLOW_HEAP_EXECUTION;
#elif defined(OS_LINUX)
  int flags = plugin_launcher.empty() ? CHILD_ALLOW_SELF : CHILD_NORMAL;
#else
  int flags = CHILD_NORMAL;
#endif

  FilePath exe_path = GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  // Put the process type and plugin path first so they're easier to see
  // in process listings using native process management tools.
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kPluginProcess);
  cmd_line->AppendSwitchPath(switches::kPluginPath, info.path);

  // Propagate the following switches to the plugin command line (along with
  // any associated values) if present in the browser command line
  static const char* const kSwitchNames[] = {
    switches::kDisableBreakpad,
#if defined(OS_MACOSX)
    switches::kDisableCompositedCoreAnimationPlugins,
#endif
    switches::kDisableLogging,
    switches::kEnableDCHECK,
    switches::kEnableLogging,
    switches::kEnableStatsTable,
    switches::kFullMemoryCrashReport,
    switches::kLoggingLevel,
    switches::kLogPluginMessages,
    switches::kNoSandbox,
    switches::kPluginStartupDialog,
    switches::kTestSandbox,
    switches::kTraceStartup,
    switches::kUseGL,
    switches::kUserAgent,
    switches::kV,
  };

  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));

  // If specified, prepend a launcher program to the command line.
  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  if (!locale.empty()) {
    // Pass on the locale so the null plugin will use the right language in the
    // prompt to install the desired plugin.
    cmd_line->AppendSwitchASCII(switches::kLang, locale);
  }

  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

#if defined(OS_POSIX)
  base::environment_vector env;
#if defined(OS_MACOSX) && !defined(__LP64__)
  // Add our interposing library for Carbon. This is stripped back out in
  // plugin_main.cc, so changes here should be reflected there.
  std::string interpose_list(plugin_interpose_strings::kInterposeLibraryPath);
  const char* existing_list =
      getenv(plugin_interpose_strings::kDYLDInsertLibrariesKey);
  if (existing_list) {
    interpose_list.insert(0, ":");
    interpose_list.insert(0, existing_list);
  }
  env.push_back(std::pair<std::string, std::string>(
      plugin_interpose_strings::kDYLDInsertLibrariesKey,
      interpose_list));
#endif
#endif

  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      false,
      env,
#endif
      cmd_line);

  // The plugin needs to be shutdown gracefully, i.e. NP_Shutdown needs to be
  // called on the plugin. The plugin process exits when it receives the
  // OnChannelError notification indicating that the browser plugin channel has
  // been destroyed.
  SetTerminateChildOnShutdown(false);

  content::GetContentClient()->browser()->PluginProcessHostCreated(this);

  return true;
}

void PluginProcessHost::ForceShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new PluginProcessMsg_NotifyRenderersOfPendingShutdown());
  BrowserChildProcessHost::ForceShutdown();
}

bool PluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ChannelCreated, OnChannelCreated)
#if defined(OS_WIN) && !defined(USE_AURA)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginWindowDestroyed,
                        OnPluginWindowDestroyed)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ReparentPluginWindow,
                        OnReparentPluginWindow)
#endif
#if defined(TOOLKIT_USES_GTK)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_MapNativeViewId,
                        OnMapNativeViewId)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginSelectWindow,
                        OnPluginSelectWindow)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginShowWindow,
                        OnPluginShowWindow)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginHideWindow,
                        OnPluginHideWindow)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginSetCursorVisibility,
                        OnPluginSetCursorVisibility)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void PluginProcessHost::OnChannelConnected(int32 peer_pid) {
  BrowserChildProcessHost::OnChannelConnected(peer_pid);
  for (size_t i = 0; i < pending_requests_.size(); ++i) {
    RequestPluginChannel(pending_requests_[i]);
  }

  pending_requests_.clear();
}

void PluginProcessHost::OnChannelError() {
  CancelRequests();
}

bool PluginProcessHost::CanShutdown() {
  return sent_requests_.empty();
}

void PluginProcessHost::CancelRequests() {
  for (size_t i = 0; i < pending_requests_.size(); ++i)
    pending_requests_[i]->OnError();
  pending_requests_.clear();

  while (!sent_requests_.empty()) {
    Client* client = sent_requests_.front();
    if (client)
      client->OnError();
    sent_requests_.pop_front();
  }
}

// static
void PluginProcessHost::CancelPendingRequestsForResourceContext(
    const content::ResourceContext* context) {
  for (BrowserChildProcessHost::Iterator host_it(
           ChildProcessInfo::PLUGIN_PROCESS);
       !host_it.Done(); ++host_it) {
    PluginProcessHost* host = static_cast<PluginProcessHost*>(*host_it);
    for (size_t i = 0; i < host->pending_requests_.size(); ++i) {
      if (&host->pending_requests_[i]->GetResourceContext() == context) {
        host->pending_requests_[i]->OnError();
        host->pending_requests_.erase(host->pending_requests_.begin() + i);
        --i;
      }
    }
  }
}

void PluginProcessHost::OpenChannelToPlugin(Client* client) {
  Notify(content::NOTIFICATION_CHILD_INSTANCE_CREATED);
  client->SetPluginInfo(info_);
  if (opening_channel()) {
    // The channel is already in the process of being opened.  Put
    // this "open channel" request into a queue of requests that will
    // be run once the channel is open.
    pending_requests_.push_back(client);
    return;
  }

  // We already have an open channel, send a request right away to plugin.
  RequestPluginChannel(client);
}

void PluginProcessHost::CancelPendingRequest(Client* client) {
  std::vector<Client*>::iterator it = pending_requests_.begin();
  while (it != pending_requests_.end()) {
    if (client == *it) {
      pending_requests_.erase(it);
      return;
    }
    ++it;
  }
  DCHECK(it != pending_requests_.end());
}

void PluginProcessHost::CancelSentRequest(Client* client) {
  std::list<Client*>::iterator it = sent_requests_.begin();
  while (it != sent_requests_.end()) {
    if (client == *it) {
      *it = NULL;
      return;
    }
    ++it;
  }
  DCHECK(it != sent_requests_.end());
}

void PluginProcessHost::RequestPluginChannel(Client* client) {
  // We can't send any sync messages from the browser because it might lead to
  // a hang.  However this async messages must be answered right away by the
  // plugin process (i.e. unblocks a Send() call like a sync message) otherwise
  // a deadlock can occur if the plugin creation request from the renderer is
  // a result of a sync message by the plugin process.
  PluginProcessMsg_CreateChannel* msg =
      new PluginProcessMsg_CreateChannel(
          client->ID(),
          client->OffTheRecord());
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push_back(client);
    client->OnSentPluginChannelRequest();
  } else {
    client->OnError();
  }
}

void PluginProcessHost::OnChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  Client* client = sent_requests_.front();

  if (client)
    client->OnChannelOpened(channel_handle);
  sent_requests_.pop_front();
}
