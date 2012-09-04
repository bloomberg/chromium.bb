// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_vector.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/utility_messages.h"
#include "content/common/webkitplatformsupport_impl.h"
#include "content/public/utility/content_utility_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "webkit/plugins/npapi/plugin_list.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>

#include "ui/gfx/gtk_util.h"
#endif

namespace {

template<typename SRC, typename DEST>
void ConvertVector(const SRC& src, DEST* dest) {
  dest->reserve(src.size());
  for (typename SRC::const_iterator i = src.begin(); i != src.end(); ++i)
    dest->push_back(typename DEST::value_type(*i));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl()
    : batch_mode_(false) {
  ChildProcess::current()->AddRefProcess();
  webkit_platform_support_.reset(new content::WebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());
  content::GetContentClient()->utility()->UtilityThreadStarted();
}

UtilityThreadImpl::~UtilityThreadImpl() {
  WebKit::shutdown();
}

bool UtilityThreadImpl::Send(IPC::Message* msg) {
  return ChildThread::Send(msg);
}

void UtilityThreadImpl::ReleaseProcessIfNeeded() {
  if (!batch_mode_)
    ChildProcess::current()->ReleaseProcess();
}

#if defined(OS_WIN)

void UtilityThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void UtilityThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}

#endif  // OS_WIN


bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  if (content::GetContentClient()->utility()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThreadImpl, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Started, OnBatchModeStarted)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Finished, OnBatchModeFinished)
#if defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(UtilityMsg_LoadPlugins, OnLoadPlugins)
#endif  // OS_POSIX
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityThreadImpl::OnBatchModeStarted() {
  batch_mode_ = true;
}

void UtilityThreadImpl::OnBatchModeFinished() {
  ChildProcess::current()->ReleaseProcess();
}

#if defined(OS_POSIX)
void UtilityThreadImpl::OnLoadPlugins(
    const std::vector<FilePath>& plugin_paths) {
  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();

  // On Linux, some plugins expect the browser to have loaded glib/gtk. Do that
  // before attempting to call into the plugin.
  // g_thread_init API is deprecated since glib 2.31.0, please see release note:
  // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if defined(TOOLKIT_GTK)
#if !(GLIB_CHECK_VERSION(2, 31, 0))
  if (!g_thread_get_initialized()) {
    g_thread_init(NULL);
  }
#endif
  gfx::GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());
#endif

  std::vector<webkit::WebPluginInfo> plugins;
  // TODO(bauerb): If we restart loading plug-ins, we might mess up the logic in
  // PluginList::ShouldLoadPlugin due to missing the previously loaded plug-ins
  // in |plugin_groups|.
  for (size_t i = 0; i < plugin_paths.size(); ++i) {
    webkit::WebPluginInfo plugin;
    if (!plugin_list->LoadPluginIntoPluginList(
        plugin_paths[i], &plugins, &plugin))
      Send(new UtilityHostMsg_LoadPluginFailed(i, plugin_paths[i]));
    else
      Send(new UtilityHostMsg_LoadedPlugin(i, plugin));
  }

  ReleaseProcessIfNeeded();
}
#endif
