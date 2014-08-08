// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_
#define CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

class GuestViewBase;
class GuestViewManagerFactory;
class GURL;

namespace content {
class BrowserContext;
}  // namespace content

class GuestViewManager : public content::BrowserPluginGuestManager,
                         public base::SupportsUserData::Data {
 public:
  explicit GuestViewManager(content::BrowserContext* context);
  virtual ~GuestViewManager();

  static GuestViewManager* FromBrowserContext(content::BrowserContext* context);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(GuestViewManagerFactory* factory) {
    GuestViewManager::factory_ = factory;
  }
  // Returns the guest WebContents associated with the given |guest_instance_id|
  // if the provided |embedder_render_process_id| is allowed to access it.
  // If the embedder is not allowed access, the embedder will be killed, and
  // this method will return NULL. If no WebContents exists with the given
  // instance ID, then NULL will also be returned.
  content::WebContents* GetGuestByInstanceIDSafely(
      int guest_instance_id,
      int embedder_render_process_id);

  int GetNextInstanceID();

  typedef base::Callback<void(content::WebContents*)>
      WebContentsCreatedCallback;
  void CreateGuest(
      const std::string& view_type,
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const base::DictionaryValue& create_params,
      const WebContentsCreatedCallback& callback);

  content::WebContents* CreateGuestWithWebContentsParams(
      const std::string& view_type,
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const content::WebContents::CreateParams& create_params);

  content::SiteInstance* GetGuestSiteInstance(
      const GURL& guest_site);

  // BrowserPluginGuestManager implementation.
  virtual void MaybeGetGuestByInstanceIDOrKill(
      int guest_instance_id,
      int embedder_render_process_id,
      const GuestByInstanceIDCallback& callback) OVERRIDE;
  virtual bool ForEachGuest(content::WebContents* embedder_web_contents,
                            const GuestCallback& callback) OVERRIDE;

 protected:
  friend class GuestViewBase;
  FRIEND_TEST_ALL_PREFIXES(GuestViewManagerTest, AddRemove);

  // Can be overriden in tests.
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents);

  void RemoveGuest(int guest_instance_id);

  content::WebContents* GetGuestByInstanceID(int guest_instance_id);

  bool CanEmbedderAccessInstanceIDMaybeKill(
      int embedder_render_process_id,
      int guest_instance_id);

  bool CanEmbedderAccessInstanceID(int embedder_render_process_id,
                                   int guest_instance_id);

  // Returns true if |guest_instance_id| can be used to add a new guest to this
  // manager.
  // We disallow adding new guest with instance IDs that were previously removed
  // from this manager using RemoveGuest.
  bool CanUseGuestInstanceID(int guest_instance_id);

  // Static factory instance (always NULL for non-test).
  static GuestViewManagerFactory* factory_;

  // Contains guests' WebContents, mapping from their instance ids.
  typedef std::map<int, content::WebContents*> GuestInstanceMap;
  GuestInstanceMap guest_web_contents_by_instance_id_;

  int current_instance_id_;

  // Any instance ID whose number not greater than this was removed via
  // RemoveGuest.
  // This is used so that we don't have store all removed instance IDs in
  // |removed_instance_ids_|.
  int last_instance_id_removed_;
  // The remaining instance IDs that are greater than
  // |last_instance_id_removed_| are kept here.
  std::set<int> removed_instance_ids_;

  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewManager);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_
