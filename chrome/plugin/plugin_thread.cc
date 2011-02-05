// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_thread.h"

#include "build/build_config.h"

#if defined(USE_X11)
#include <gtk/gtk.h>
#elif defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/process_util.h"
#include "base/threading/thread_local.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/chrome_plugin_host.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/render_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "net/base/net_errors.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

static base::LazyInstance<base::ThreadLocalPointer<PluginThread> > lazy_tls(
    base::LINKER_INITIALIZED);

PluginThread::PluginThread()
    : preloaded_plugin_module_(NULL) {
  plugin_path_ =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kPluginPath);

  lazy_tls.Pointer()->Set(this);
#if defined(OS_LINUX)
  {
    // XEmbed plugins assume they are hosted in a Gtk application, so we need
    // to initialize Gtk in the plugin process.
    g_thread_init(NULL);

    // Flash has problems receiving clicks with newer GTKs due to the
    // client-side windows change.  To be safe, we just always set the
    // backwards-compat environment variable.
    setenv("GDK_NATIVE_WINDOWS", "1", 1);

    gfx::GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());

    // GTK after 2.18 resets the environment variable.  But if we're using
    // nspluginwrapper, that means it'll spawn its subprocess without the
    // environment variable!  So set it again.
    setenv("GDK_NATIVE_WINDOWS", "1", 1);
  }

  ui::SetDefaultX11ErrorHandlers();
#endif

  PatchNPNFunctions();

  // Preload the library to avoid loading, unloading then reloading
  preloaded_plugin_module_ = base::LoadNativeLibrary(plugin_path_);

  ChromePluginLib::Create(plugin_path_, GetCPBrowserFuncsForPlugin());

  scoped_refptr<webkit::npapi::PluginLib> plugin(
      webkit::npapi::PluginLib::CreatePluginLib(plugin_path_));
  if (plugin.get()) {
    plugin->NP_Initialize();

#if defined(OS_MACOSX)
    base::mac::ScopedCFTypeRef<CFStringRef> plugin_name(
        base::SysUTF16ToCFStringRef(plugin->plugin_info().name));
    base::mac::ScopedCFTypeRef<CFStringRef> app_name(
        base::SysUTF16ToCFStringRef(
            l10n_util::GetStringUTF16(IDS_SHORT_PLUGIN_APP_NAME)));
    base::mac::ScopedCFTypeRef<CFStringRef> process_name(
        CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ (%@)"),
                                 plugin_name.get(), app_name.get()));
    base::mac::SetProcessName(process_name);
#endif
  }

  // Certain plugins, such as flash, steal the unhandled exception filter
  // thus we never get crash reports when they fault. This call fixes it.
  message_loop()->set_exception_restoration(true);
}

PluginThread::~PluginThread() {
  if (preloaded_plugin_module_) {
    base::UnloadNativeLibrary(preloaded_plugin_module_);
    preloaded_plugin_module_ = NULL;
  }
  PluginChannelBase::CleanupChannels();
  webkit::npapi::PluginLib::UnloadAllPlugins();
  ChromePluginLib::UnloadAllPlugins();

  if (webkit_glue::ShouldForcefullyTerminatePluginProcess())
    base::KillProcess(base::GetCurrentProcessHandle(), 0, /* wait= */ false);

  lazy_tls.Pointer()->Set(NULL);
}

PluginThread* PluginThread::current() {
  return lazy_tls.Pointer()->Get();
}

bool PluginThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginThread, msg)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_PluginMessage, OnPluginMessage)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_NotifyRenderersOfPendingShutdown,
                        OnNotifyRenderersOfPendingShutdown)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginThread::OnCreateChannel(int renderer_id,
                                   bool off_the_record) {
  scoped_refptr<PluginChannel> channel(PluginChannel::GetPluginChannel(
      renderer_id, ChildProcess::current()->io_message_loop()));
  IPC::ChannelHandle channel_handle;
  if (channel.get()) {
    channel_handle.name = channel->channel_handle().name;
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD.
    channel_handle.socket = base::FileDescriptor(channel->renderer_fd(), false);
#endif
    channel->set_off_the_record(off_the_record);
  }

  Send(new PluginProcessHostMsg_ChannelCreated(channel_handle));
}

void PluginThread::OnPluginMessage(const std::vector<unsigned char> &data) {
  // We Add/Release ref here to ensure that something will trigger the
  // shutdown mechanism for processes started in the absence of renderer's
  // opening a plugin channel.
  ChildProcess::current()->AddRefProcess();
  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path_);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
  ChildProcess::current()->ReleaseProcess();
}

void PluginThread::OnNotifyRenderersOfPendingShutdown() {
  PluginChannel::NotifyRenderersOfPendingShutdown();
}

namespace webkit_glue {

#if defined(OS_WIN)
bool DownloadUrl(const std::string& url, HWND caller_window) {
  PluginThread* plugin_thread = PluginThread::current();
  if (!plugin_thread) {
    return false;
  }

  IPC::Message* message =
      new PluginProcessHostMsg_DownloadUrl(MSG_ROUTING_NONE, url,
                                           ::GetCurrentProcessId(),
                                           caller_window);
  return plugin_thread->Send(message);
}
#endif

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  if (!plugin_finder_url) {
    NOTREACHED();
    return false;
  }

  PluginThread* plugin_thread = PluginThread::current();
  if (!plugin_thread)
    return false;

  plugin_thread->Send(
      new PluginProcessHostMsg_GetPluginFinderUrl(plugin_finder_url));
  DCHECK(!plugin_finder_url->empty());
  return true;
}

bool IsDefaultPluginEnabled() {
  return true;
}

// Dispatch the resolve proxy resquest to the right code, depending on which
// process the plugin is running in {renderer, browser, plugin}.
bool FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  int net_error;
  std::string proxy_result;

  bool result;
  if (IsPluginProcess()) {
    result = PluginThread::current()->Send(
        new PluginProcessHostMsg_ResolveProxy(url, &net_error, &proxy_result));
  } else {
    result = RenderThread::current()->Send(
        new ViewHostMsg_ResolveProxy(url, &net_error, &proxy_result));
  }

  if (!result || net_error != net::OK)
    return false;

  *proxy_list = proxy_result;
  return true;
}

}  // namespace webkit_glue
