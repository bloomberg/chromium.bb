// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_PROCESS_HOST_H_
#define CHROME_BROWSER_PLUGIN_PROCESS_HOST_H_
#pragma once

#include "build/build_config.h"

#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser_child_process_host.h"
#include "chrome/browser/net/resolve_proxy_msg_helper.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/plugins/npapi/webplugininfo.h"

namespace gfx {
class Rect;
}

namespace IPC {
struct ChannelHandle;
}

class GURL;

// Represents the browser side of the browser <--> plugin communication
// channel.  Different plugins run in their own process, but multiple instances
// of the same plugin run in the same process.  There will be one
// PluginProcessHost per plugin process, matched with a corresponding
// PluginProcess running in the plugin process.  The browser is responsible for
// starting the plugin process when a plugin is created that doesn't already
// have a process.  After that, most of the communication is directly between
// the renderer and plugin processes.
class PluginProcessHost : public BrowserChildProcessHost,
                          public ResolveProxyMsgHelper::Delegate {
 public:
  class Client {
   public:
    // Returns a opaque unique identifier for the process requesting
    // the channel.
    virtual int ID() = 0;
    virtual bool OffTheRecord() = 0;
    virtual void SetPluginInfo(const webkit::npapi::WebPluginInfo& info) = 0;
    // The client should delete itself when one of these methods is called.
    virtual void OnChannelOpened(const IPC::ChannelHandle& handle) = 0;
    virtual void OnError() = 0;

   protected:
    virtual ~Client() {}
  };

  PluginProcessHost();
  virtual ~PluginProcessHost();

  // Initialize the new plugin process, returning true on success. This must
  // be called before the object can be used.
  bool Init(const webkit::npapi::WebPluginInfo& info, const std::string& locale);

  // Force the plugin process to shutdown (cleanly).
  virtual void ForceShutdown();

  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // ResolveProxyMsgHelper::Delegate implementation:
  virtual void OnResolveProxyCompleted(IPC::Message* reply_msg,
                                       int result,
                                       const std::string& proxy_list);

  // Tells the plugin process to create a new channel for communication with a
  // renderer.  When the plugin process responds with the channel name,
  // OnChannelOpened in the client is called.
  void OpenChannelToPlugin(Client* client);

  // This function is called on the IO thread once we receive a reply from the
  // modal HTML dialog (in the form of a JSON string). This function forwards
  // that reply back to the plugin that requested the dialog.
  void OnModalDialogResponse(const std::string& json_retval,
                             IPC::Message* sync_result);

#if defined(OS_MACOSX)
  // This function is called on the IO thread when the browser becomes the
  // active application.
  void OnAppActivation();
#endif

  const webkit::npapi::WebPluginInfo& info() const { return info_; }

#if defined(OS_WIN)
  // Tracks plugin parent windows created on the browser UI thread.
  void AddWindow(HWND window);
#endif

 private:
  friend class PluginResolveProxyHelper;

  // Sends a message to the plugin process to request creation of a new channel
  // for the given mime type.
  void RequestPluginChannel(Client* client);

  virtual void OnProcessLaunched();

  // Message handlers.
  void OnChannelCreated(const IPC::ChannelHandle& channel_handle);
  void OnGetPluginFinderUrl(std::string* plugin_finder_url);
  void OnGetCookies(uint32 request_context, const GURL& url,
                    std::string* cookies);
  void OnAccessFiles(int renderer_id, const std::vector<std::string>& files,
                     bool* allowed);
  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);
  void OnPluginMessage(const std::vector<uint8>& data);

#if defined(OS_WIN)
  void OnPluginWindowDestroyed(HWND window, HWND parent);
  void OnDownloadUrl(const std::string& url, int source_child_unique_id,
                     gfx::NativeWindow caller_window);
#endif

#if defined(USE_X11)
  void OnMapNativeViewId(gfx::NativeViewId id, gfx::PluginWindowHandle* output);
#endif

#if defined(OS_MACOSX)
  void OnPluginSelectWindow(uint32 window_id, gfx::Rect window_rect,
                            bool modal);
  void OnPluginShowWindow(uint32 window_id, gfx::Rect window_rect,
                          bool modal);
  void OnPluginHideWindow(uint32 window_id, gfx::Rect window_rect);
  void OnPluginSetCursorVisibility(bool visible);
#endif

  virtual bool CanShutdown();

  void CancelRequests();

  // These are channel requests that we are waiting to send to the
  // plugin process once the channel is opened.
  std::vector<Client*> pending_requests_;

  // These are the channel requests that we have already sent to
  // the plugin process, but haven't heard back about yet.
  std::queue<Client*> sent_requests_;

  // Information about the plugin.
  webkit::npapi::WebPluginInfo info_;

  // Helper class for handling PluginProcessHost_ResolveProxy messages (manages
  // the requests to the proxy service).
  ResolveProxyMsgHelper resolve_proxy_msg_helper_;

#if defined(OS_WIN)
  // Tracks plugin parent windows created on the UI thread.
  std::set<HWND> plugin_parent_windows_set_;
#endif
#if defined(OS_MACOSX)
  // Tracks plugin windows currently visible.
  std::set<uint32> plugin_visible_windows_set_;
  // Tracks full screen windows currently visible.
  std::set<uint32> plugin_fullscreen_windows_set_;
  // Tracks modal windows currently visible.
  std::set<uint32> plugin_modal_windows_set_;
  // Tracks the current visibility of the cursor.
  bool plugin_cursor_visible_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginProcessHost);
};

#endif  // CHROME_BROWSER_PLUGIN_PROCESS_HOST_H_
