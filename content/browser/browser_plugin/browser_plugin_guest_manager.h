// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuestManager serves as a message router to BrowserPluginGuests
// for all guests within a given profile.
// Messages are routed to a particular guest instance based on an instance_id.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_MANAGER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_MANAGER_H_

#include "base/basictypes.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

struct BrowserPluginHostMsg_CreateGuest_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
class GURL;

namespace gfx {
class Point;
}

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

class BrowserPluginGuest;
class BrowserPluginHostFactory;
class RenderProcessHostImpl;
class SiteInstance;
class WebContents;
class WebContentsImpl;

class CONTENT_EXPORT BrowserPluginGuestManager :
    public base::SupportsUserData::Data {
 public:
  virtual ~BrowserPluginGuestManager();

  static BrowserPluginGuestManager* Create();

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    content::BrowserPluginGuestManager::factory_ = factory;
  }

  // Gets the next available instance id.
  int get_next_instance_id() { return ++next_instance_id_; }

  // Creates a guest WebContents with the provided |instance_id| and |params|.
  // If params.src is present, the new guest will also be navigated to the
  // provided URL. Optionally, the new guest may be attached to a
  // |guest_opener|, and may be attached to a pre-selected |routing_id|.
  BrowserPluginGuest* CreateGuest(
      SiteInstance* embedder_site_instance,
      int instance_id,
      int routing_id,
      BrowserPluginGuest* guest_opener,
      const BrowserPluginHostMsg_CreateGuest_Params& params);

  // Returns a BrowserPluginGuest given an instance ID. Returns NULL if the
  // guest wasn't found.  If the guest doesn't belong to the given embedder,
  // then NULL is returned and the embedder is killed.
  BrowserPluginGuest* GetGuestByInstanceID(
      int instance_id,
      int embedder_render_process_id) const;

  // Adds a new |guest_web_contents| to the embedder (overridable in test).
  virtual void AddGuest(int instance_id, WebContentsImpl* guest_web_contents);
  void RemoveGuest(int instance_id);

  void OnMessageReceived(const IPC::Message& message, int render_process_id);

 private:
  friend class TestBrowserPluginGuestManager;

  BrowserPluginGuestManager();

  // Returns whether the given embedder process is allowed to access |guest|.
  static bool CanEmbedderAccessGuest(int embedder_render_process_id,
                                     BrowserPluginGuest* guest);

  // Returns an existing SiteInstance if the current profile has a guest of the
  // given |guest_site|.
  SiteInstance* GetGuestSiteInstance(const GURL& guest_site);

  // Message handlers.
  void OnUnhandledSwapBuffersACK(int instance_id,
                                 int route_id,
                                 int gpu_host_id,
                                 const std::string& mailbox_name,
                                 uint32 sync_point);

  // Static factory instance (always NULL outside of tests).
  static BrowserPluginHostFactory* factory_;

  // Contains guests' WebContents, mapping from their instance ids.
  typedef std::map<int, WebContentsImpl*> GuestInstanceMap;
  GuestInstanceMap guest_web_contents_by_instance_id_;
  int next_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuestManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_MANAGER_H_

