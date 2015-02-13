// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions{
class GuestViewBase;
class GuestViewManagerFactory;

class GuestViewManager : public content::BrowserPluginGuestManager,
                         public base::SupportsUserData::Data {
 public:
  explicit GuestViewManager(content::BrowserContext* context);
  ~GuestViewManager() override;

  // Returns the GuestViewManager associated with |context|. If one isn't
  // available, then it is created and returned.
  static GuestViewManager* FromBrowserContext(content::BrowserContext* context);

  // Returns the GuestViewManager associated with |context|. If one isn't
  // available, then nullptr is returned.
  static GuestViewManager* FromBrowserContextIfAvailable(
      content::BrowserContext* context);

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

  // Associates the Browser Plugin with |element_instance_id| to a
  // guest that has ID of |guest_instance_id| and sets initialization
  // parameters, |params| for it.
  void AttachGuest(int embedder_process_id,
                   int element_instance_id,
                   int guest_instance_id,
                   const base::DictionaryValue& attach_params);

  // Removes the association between |element_instance_id| and a guest instance
  // ID if one exists.
  void DetachGuest(GuestViewBase* guest);

  int GetNextInstanceID();
  int GetGuestInstanceIDForElementID(
      int owner_process_id,
      int element_instance_id);

  using WebContentsCreatedCallback =
      base::Callback<void(content::WebContents*)>;
  void CreateGuest(const std::string& view_type,
                   content::WebContents* owner_web_contents,
                   const base::DictionaryValue& create_params,
                   const WebContentsCreatedCallback& callback);

  content::WebContents* CreateGuestWithWebContentsParams(
      const std::string& view_type,
      content::WebContents* owner_web_contents,
      const content::WebContents::CreateParams& create_params);

  content::SiteInstance* GetGuestSiteInstance(
      const GURL& guest_site);

  // BrowserPluginGuestManager implementation.
  content::WebContents* GetGuestByInstanceID(
      int owner_process_id,
      int element_instance_id) override;
  bool ForEachGuest(content::WebContents* owner_web_contents,
                    const GuestCallback& callback) override;

 protected:
  friend class GuestViewBase;
  FRIEND_TEST_ALL_PREFIXES(GuestViewManagerTest, AddRemove);

  // Can be overriden in tests.
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents);

  // Can be overriden in tests.
  virtual void RemoveGuest(int guest_instance_id);

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
  using GuestInstanceMap = std::map<int, content::WebContents*>;
  GuestInstanceMap guest_web_contents_by_instance_id_;

  struct ElementInstanceKey {
    int embedder_process_id;
    int element_instance_id;

    ElementInstanceKey();
    ElementInstanceKey(int embedder_process_id,
                       int element_instance_id);

    bool operator<(const ElementInstanceKey& other) const;
    bool operator==(const ElementInstanceKey& other) const;
  };

  using GuestInstanceIDMap = std::map<ElementInstanceKey, int>;
  GuestInstanceIDMap instance_id_map_;

  // The reverse map of GuestInstanceIDMap.
  using GuestInstanceIDReverseMap = std::map<int, ElementInstanceKey>;
  GuestInstanceIDReverseMap reverse_instance_id_map_;

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

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_
