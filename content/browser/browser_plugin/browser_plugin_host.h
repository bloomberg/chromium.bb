// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_HOST_H__
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_HOST_H__
#pragma once

#include <map>

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ppapi/c/pp_instance.h"
#include "ui/gfx/size.h"

class WebContentsImpl;

namespace content {

class RenderProcessHost;

// A BrowserPluginHost object is used by both embedders and guests.
// The primary purpose of BrowserPluginHost is to allow an embedder
// to manage the lifetime of its guests. It cleans up its guests
// on navigation, crashes, and "hides" guests when it hides.
// For the guest, BrowserPluginHost keeps track of its embedder,
// its BrowserPlugin instance ID, and some initialization state
// such as initial size.
class BrowserPluginHost : public WebContentsObserver,
                          public NotificationObserver,
                          public WebContentsDelegate {
 public:
  // BrowserPluginHost is owned by a WebContentsImpl. Here it takes in its
  // owner. The owner can be either a guest or embedder WebContents.
  explicit BrowserPluginHost(WebContentsImpl* web_contents);

  virtual ~BrowserPluginHost();

  // TODO(fsamuel): Remove this once BrowserPluginHost_MapInstance
  // is removed.
  void OnPendingNavigation(RenderViewHost* dest_rvh);

  void ConnectEmbedderToChannel(RenderViewHost* render_view_host,
                                const IPC::ChannelHandle& handle);

  // This is called on the first navigation of the browser plugin. It creates
  // a new WebContentsImpl for the guest, associates it with its embedder, sets
  // its size and navigates it to the src URL.
  void NavigateGuestFromEmbedder(RenderViewHost* render_view_host,
                                 int container_instance_id,
                                 long long frame_id,
                                 const std::string& src,
                                 const gfx::Size& size);

  RenderProcessHost* embedder_render_process_host() const {
    return embedder_render_process_host_;
  }

 private:
  typedef std::map<WebContentsImpl*, int64> GuestMap;
  typedef std::map<int, BrowserPluginHost*> ContainerInstanceMap;

  // Get a guest BrowserPluginHost by its container ID specified
  // in BrowserPlugin.
  BrowserPluginHost* GetGuestByContainerID(int container_id);

  // Associate a guest with its container instance ID.
  void RegisterContainerInstance(
      int container_id,
      BrowserPluginHost* observer);

  // An embedder BrowserPluginHost keeps track of
  // its guests so that if it navigates away, its associated RenderView
  // crashes or it is hidden, it takes appropriate action on the guest.
  void AddGuest(WebContentsImpl* guest, int64 frame_id);

  // This removes a guest from an embedder's guest list.
  // TODO(fsamuel): Use this when deleting guest after they crash.
  // ToT plugin crash handling seems to be broken in this case and so I can't
  // test this scenario at the moment. Delete this comment once fixed.
  void RemoveGuest(WebContentsImpl* guest);

  void set_embedder_render_process_host(
      RenderProcessHost* embedder_render_process_host) {
    embedder_render_process_host_ = embedder_render_process_host;
  }
  int instance_id() const { return instance_id_; }
  void set_instance_id(int instance_id) { instance_id_ = instance_id; }
  const gfx::Size& initial_size() const { return initial_size_; }
  void set_initial_size(const gfx::Size& size) { initial_size_ = size; }
  RenderViewHost* pending_render_view_host() const {
    return pending_render_view_host_;
  }

  void OnNavigateFromGuest(PP_Instance instance,
                           const std::string& src);

  void DestroyGuests();

  // TODO(fsamuel): Replace BrowserPluginHost_MapInstance with a message
  // over the GuestToEmbedderChannel that tells the guest to size itself
  // and begin compositing. Currently we use the guest's routing ID to look
  // up the appropriate guest in
  // BrowserPluginChannelManager::OnCompleteNavigation. In order to bypass the
  // need to grab a routing ID from the browser process, we can instead pass the
  // container instance ID to the guest on ViewMsg_New. The container instance
  // ID and the embedder channel name can then be used together to uniquely
  // identify a guest RenderViewImpl within a render process.
  void OnMapInstance(int container_instance_id, PP_Instance instance);

  // WebContentObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  // Used to monitor frame navigation to cleanup guests when a frame navigates
  // away from the browser plugin it's hosting.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

  // NotificationObserver method override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;
  RenderProcessHost* embedder_render_process_host_;
  // An identifier that uniquely identifies a browser plugin container
  // within an embedder.
  int instance_id_;
  gfx::Size initial_size_;
  GuestMap guests_;
  ContainerInstanceMap guests_by_container_id_;
  // TODO(fsamuel): This should not be exposed outside of WebContentsImpl
  // because this can change at any time. Remove this, along with
  // OnPendingNavigation once BrowserPluginHost_MapInstance is modified
  // to be sent over the GuestToEmbedderChannel directly instead of through
  // the browser process.
  RenderViewHost* pending_render_view_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_HOST_H_
