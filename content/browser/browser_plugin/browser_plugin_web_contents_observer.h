// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_WEB_CONTENTS_OBSERVER_H__
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_WEB_CONTENTS_OBSERVER_H__
#pragma once

#include <map>

#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/size.h"

class WebContentsImpl;

namespace content {

class BrowserPluginWebContentsObserver: public WebContentsObserver,
                                        public NotificationObserver {
 public:
  BrowserPluginWebContentsObserver(WebContentsImpl* web_contents);

  virtual ~BrowserPluginWebContentsObserver();

  // A Host BrowserPluginWebContentsObserver keeps track of
  // its guests so that if it navigates away, its associated RenderView
  // crashes or it is hidden, it takes appropriate action on the guest.
  void AddGuest(WebContentsImpl* guest, int64 frame_id);

  void RemoveGuest(WebContentsImpl* guest);

  WebContentsImpl* host() const { return host_; }

  void set_host(WebContentsImpl* host) { host_ = host; }

  int instance_id() const { return instance_id_; }

  void set_instance_id(int instance_id) { instance_id_ = instance_id; }

  // WebContentObserver implementation.

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type) OVERRIDE;

  virtual void RenderViewDeleted(
      RenderViewHost* render_view_host) OVERRIDE;

  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  virtual void WebContentsDestroyed(WebContents* tab) OVERRIDE;

 private:
  typedef std::map<WebContentsImpl*, int64> GuestMap;

  void OnOpenChannelToBrowserPlugin(int32 instance_id,
                                    long long frame_id,
                                    const std::string& src,
                                    const gfx::Size& size);

  void OnGuestReady();

  void OnResizeGuest(int width, int height);

  void OnRendererPluginChannelCreated(const IPC::ChannelHandle& handle);

  void DestroyGuests();

  // NotificationObserver method override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  WebContentsImpl* host_;

  // An identifier that uniquely identifies a browser plugin container
  // within a host.
  int instance_id_;

  GuestMap guests_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_WEB_CONTENTS_OBSERVER_H_
