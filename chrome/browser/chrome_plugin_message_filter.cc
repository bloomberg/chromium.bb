// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_message_filter.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugin_download_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/plugin_process_host.h"
#include "content/common/plugin_messages.h"
#include "net/url_request/url_request_context_getter.h"

static const char kDefaultPluginFinderURL[] =
    "https://dl-ssl.google.com/edgedl/chrome/plugins/plugins2.xml";

ChromePluginMessageFilter::ChromePluginMessageFilter(PluginProcessHost* process)
    : process_(process) {
}

ChromePluginMessageFilter::~ChromePluginMessageFilter() {
}

bool ChromePluginMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromePluginMessageFilter, message)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_DownloadUrl, OnDownloadUrl)
#endif
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_GetPluginFinderUrl,
                        OnGetPluginFinderUrl)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool ChromePluginMessageFilter::Send(IPC::Message* message) {
  return process_->Send(message);
}

#if defined(OS_WIN)
void ChromePluginMessageFilter::OnDownloadUrl(const std::string& url,
                                              int source_pid,
                                              gfx::NativeWindow caller_window) {
  PluginDownloadUrlHelper* download_url_helper =
      new PluginDownloadUrlHelper(url, source_pid, caller_window, NULL);
  download_url_helper->InitiateDownload(
      Profile::GetDefaultRequestContext()->GetURLRequestContext());
}
#endif

void ChromePluginMessageFilter::OnGetPluginFinderUrl(
    std::string* plugin_finder_url) {
  // TODO(bauerb): Move this method to MessageFilter.
  if (!g_browser_process->plugin_finder_disabled()) {
    // TODO(iyengar): Add the plumbing to retrieve the default
    // plugin finder URL.
    *plugin_finder_url = kDefaultPluginFinderURL;
  } else {
    plugin_finder_url->clear();
  }
}
