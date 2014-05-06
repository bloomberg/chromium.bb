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
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_plugin_guest_manager_delegate.h"
#include "ipc/ipc_message.h"

struct BrowserPluginHostMsg_Attach_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
class GURL;

namespace gfx {
class Point;
}

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

class BrowserContext;
class BrowserPluginGuest;
class BrowserPluginHostFactory;
class RenderWidgetHostImpl;
class SiteInstance;
class WebContents;

// WARNING: All APIs should be guarded with a process ID check like
// CanEmbedderAccessInstanceIDMaybeKill, to prevent abuse by normal renderer
// processes.
class CONTENT_EXPORT BrowserPluginGuestManager :
    public base::SupportsUserData::Data {
 public:
  virtual ~BrowserPluginGuestManager();

  static BrowserPluginGuestManager* FromBrowserContext(BrowserContext* context);

  BrowserPluginGuestManagerDelegate* GetDelegate() const;

  static BrowserPluginGuestManager* Create(BrowserContext* context);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    content::BrowserPluginGuestManager::factory_ = factory;
  }

  // Gets the next available instance id. 0 is considered an invalid instance
  // ID.
  int GetNextInstanceID();

  // Creates a guest WebContents with the provided |instance_id| and |params|.
  // If params.src is present, the new guest will also be navigated to the
  // provided URL. Optionally, the new guest may be attached to a
  // |guest_opener|, and may be attached to a pre-selected |routing_id|.
  BrowserPluginGuest* CreateGuest(
      SiteInstance* embedder_site_instance,
      int instance_id,
      const BrowserPluginHostMsg_Attach_Params& params,
      scoped_ptr<base::DictionaryValue> extra_params);

  // Returns a BrowserPluginGuest given an |instance_id|. Returns NULL if the
  // guest wasn't found.  If the embedder is not permitted to access the given
  // |instance_id|, the embedder is killed, and NULL is returned.
  BrowserPluginGuest* GetGuestByInstanceID(
      int instance_id,
      int embedder_render_process_id) const;

  // Adds a new |guest_web_contents| to the embedder (overridable in test).
  virtual void AddGuest(int instance_id, WebContents* guest_web_contents);

  // Removes the guest with the given |instance_id| from this
  // BrowserPluginGuestManager.
  void RemoveGuest(int instance_id);

  // Returns whether the specified embedder is permitted to access the given
  // |instance_id|, and kills the embedder if not.
  bool CanEmbedderAccessInstanceIDMaybeKill(int embedder_render_process_id,
                                            int instance_id) const;

  typedef base::Callback<bool(BrowserPluginGuest*)> GuestCallback;
  bool ForEachGuest(WebContents* embedder_web_contents,
                    const GuestCallback& callback);

  void OnMessageReceived(const IPC::Message& message, int render_process_id);

 private:
  friend class TestBrowserPluginGuestManager;

  explicit BrowserPluginGuestManager(BrowserContext* context);

  // Returns an existing SiteInstance if the current profile has a guest of the
  // given |guest_site|.
  SiteInstance* GetGuestSiteInstance(const GURL& guest_site);

  // Static factory instance (always NULL outside of tests).
  static BrowserPluginHostFactory* factory_;

  // The BrowserContext in which this manager this stored.
  BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuestManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_MANAGER_H_
