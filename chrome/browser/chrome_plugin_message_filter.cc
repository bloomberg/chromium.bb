// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_message_filter.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/plugin_installer_infobar_delegate.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_plugin_messages.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"

#if defined(OS_WIN)
#include "chrome/browser/plugin_download_helper.h"
#endif

using content::BrowserThread;

static const char kDefaultPluginFinderURL[] =
    "https://ssl.gstatic.com/chrome/config/plugins2.xml";

ChromePluginMessageFilter::ChromePluginMessageFilter(PluginProcessHost* process)
    : process_(process) {
}

ChromePluginMessageFilter::~ChromePluginMessageFilter() {
}

bool ChromePluginMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromePluginMessageFilter, message)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromePluginProcessHostMsg_DownloadUrl, OnDownloadUrl)
#endif
    IPC_MESSAGE_HANDLER(ChromePluginProcessHostMsg_GetPluginFinderUrl,
                        OnGetPluginFinderUrl)
    IPC_MESSAGE_HANDLER(ChromePluginProcessHostMsg_MissingPluginStatus,
                        OnMissingPluginStatus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool ChromePluginMessageFilter::Send(IPC::Message* message) {
  return process_->Send(message);
}

#if defined(OS_WIN)
void ChromePluginMessageFilter::OnDownloadUrl(const std::string& url,
                                              gfx::NativeWindow caller_window,
                                              int render_process_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromePluginMessageFilter::OnDownloadUrlOnUIThread,
                 url, caller_window,
                 render_process_id));
}

// static
void ChromePluginMessageFilter::OnDownloadUrlOnUIThread(
    const std::string& url,
    gfx::NativeWindow caller_window,
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    return;
  }
  // |download_url_helper| will delete itself after running the callback.
  PluginDownloadUrlHelper* download_url_helper =
      new PluginDownloadUrlHelper();
  download_url_helper->InitiateDownload(
      GURL(url),
      host->GetBrowserContext()->GetRequestContext(),
      base::Bind(&ChromePluginMessageFilter::PluginDownloadFinished,
                 caller_window),
      base::Bind(&ChromePluginMessageFilter::PluginDownloadError));
}

// static
void ChromePluginMessageFilter::PluginDownloadFinished(
    gfx::NativeWindow caller_window,
    const FilePath& response_file) {
  FilePath::StringType path = response_file.value();
  COPYDATASTRUCT download_file_data = {0};
  download_file_data.cbData =
      static_cast<unsigned long>((path.length() + 1) *
      sizeof(FilePath::CharType));
  download_file_data.lpData = const_cast<FilePath::CharType *>(path.c_str());
  download_file_data.dwData = !path.empty();

  if (::IsWindow(caller_window)) {
    ::SendMessage(caller_window, WM_COPYDATA, NULL,
                  reinterpret_cast<LPARAM>(&download_file_data));
  }
}

// static
void ChromePluginMessageFilter::PluginDownloadError(
    const std::string& error) {
  NOTREACHED() << error;
}
#endif  // OS_WIN

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

void ChromePluginMessageFilter::OnMissingPluginStatus(
    int status, int render_process_id, int render_view_id,
    gfx::NativeWindow window) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &ChromePluginMessageFilter::HandleMissingPluginStatus,
          status, render_process_id, render_view_id, window));
}

// static
void ChromePluginMessageFilter::HandleMissingPluginStatus(
    int status, int render_process_id, int render_view_id,
    gfx::NativeWindow window) {
// TODO(PORT): pull in when plug-ins work
#if defined(OS_WIN)
  RenderViewHost* host = RenderViewHost::FromID(render_process_id,
                                                render_view_id);
  if (!host || !host->delegate() || !host->delegate()->GetAsWebContents())
    return;

  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      host->delegate()->GetAsWebContents());
  DCHECK(tcw);
  InfoBarTabHelper* infobar_helper = tcw->infobar_tab_helper();

  if (status == webkit::npapi::default_plugin::MISSING_PLUGIN_AVAILABLE) {
    infobar_helper->AddInfoBar(new PluginInstallerInfoBarDelegate(
        NULL, infobar_helper, string16(), GURL(),
        base::Bind(&ChromePluginMessageFilter::InstallMissingPlugin,
                   base::Unretained(window))));
    return;
  }

  DCHECK_EQ(webkit::npapi::default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD,
            status);
  for (size_t i = 0; i < infobar_helper->infobar_count(); ++i) {
    InfoBarDelegate* delegate = infobar_helper->GetInfoBarDelegateAt(i);
    if (delegate->AsPluginInstallerInfoBarDelegate() != NULL) {
      infobar_helper->RemoveInfoBar(delegate);
      return;
    }
  }
#else
  // TODO(port): Implement the infobar that accompanies the default plugin.
  // Linux: http://crbug.com/10952
  // Mac: http://crbug.com/17392
  NOTIMPLEMENTED();
#endif  // OS_WIN
}

// static
void ChromePluginMessageFilter::InstallMissingPlugin(gfx::NativeWindow window) {
#if defined(OS_WIN)
  ::PostMessage(window,
                webkit::npapi::default_plugin::kInstallMissingPluginMessage,
                0,
                0);
#else
  NOTIMPLEMENTED();
#endif  // OS_WIN
}
