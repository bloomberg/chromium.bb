// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/declarative_user_script_manager.h"
#include "extensions/browser/declarative_user_script_master.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"

using content::BrowserThread;

namespace extensions {

// This observer ensures that the content scripts added by the guest are removed
// when its embedder goes away.
// The OwnerWebContentsObserver object will be destroyed when the embedder web
// contents it observed is gone.
class WebViewContentScriptManager::OwnerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  OwnerWebContentsObserver(content::WebContents* embedder_web_contents,
                           const HostID& host_id,
                           WebViewContentScriptManager* manager)
      : WebContentsObserver(embedder_web_contents),
        host_id_(host_id),
        web_view_content_script_manager_(manager) {}
  ~OwnerWebContentsObserver() override {}

  // WebContentsObserver:
  void WebContentsDestroyed() override {
    // If the embedder is destroyed then remove all the content scripts of the
    // guest.
    RemoveContentScripts();
  }
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override {
    // If the embedder navigates to a different page then remove all the content
    // scripts of the guest.
    if (details.is_navigation_to_different_page())
      RemoveContentScripts();
  }
  void RenderProcessGone(base::TerminationStatus status) override {
    // If the embedder crashes, then remove all the content scripts of the
    // guest.
    RemoveContentScripts();
  }

  void add_view_instance_id(int view_instance_id) {
    view_instance_ids_.insert(view_instance_id);
  }

 private:
  void RemoveContentScripts() {
    DCHECK(web_view_content_script_manager_);

    // Step 1: removes content scripts of all the guests embedded.
    for (int view_instance_id : view_instance_ids_) {
      web_view_content_script_manager_->RemoveContentScripts(
          web_contents(), view_instance_id, host_id_,
          std::vector<std::string>());
    }
    // Step 2: removes this observer.
    // This object can be deleted after this line.
    web_view_content_script_manager_->RemoveObserver(web_contents());
  }

  HostID host_id_;
  std::set<int> view_instance_ids_;
  WebViewContentScriptManager* web_view_content_script_manager_;

  DISALLOW_COPY_AND_ASSIGN(OwnerWebContentsObserver);
};

WebViewContentScriptManager::WebViewContentScriptManager(
    content::BrowserContext* browser_context)
    : user_script_loader_observer_(this), browser_context_(browser_context) {
}

WebViewContentScriptManager::~WebViewContentScriptManager() {
}

WebViewContentScriptManager* WebViewContentScriptManager::Get(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebViewContentScriptManager* manager =
      static_cast<WebViewContentScriptManager*>(browser_context->GetUserData(
          webview::kWebViewContentScriptManagerKeyName));
  if (!manager) {
    manager = new WebViewContentScriptManager(browser_context);
    browser_context->SetUserData(webview::kWebViewContentScriptManagerKeyName,
                                 manager);
  }
  return manager;
}

void WebViewContentScriptManager::AddContentScripts(
    content::WebContents* embedder_web_contents,
    int embedder_routing_id,
    int view_instance_id,
    const HostID& host_id,
    const std::set<UserScript>& scripts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(embedder_web_contents);

  DeclarativeUserScriptMaster* master =
      DeclarativeUserScriptManager::Get(browser_context_)
          ->GetDeclarativeUserScriptMasterByID(host_id);
  DCHECK(master);

  // We need to update WebViewRenderState in the IO thread if the guest exists.
  std::set<int> ids_to_add;

  int embedder_process_id =
      embedder_web_contents->GetRenderProcessHost()->GetID();
  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  GuestContentScriptMap::iterator iter = guest_content_script_map_.find(key);

  // Step 1: finds the entry in guest_content_script_map_ by the given |key|.
  // If there isn't any content script added for the given guest yet, insert an
  // empty map first.
  if (iter == guest_content_script_map_.end()) {
    iter = guest_content_script_map_.insert(
        iter,
        std::pair<GuestMapKey, ContentScriptMap>(key, ContentScriptMap()));
  }

  // Step 2: updates the guest_content_script_map_.
  ContentScriptMap& map = iter->second;
  std::set<UserScript> scripts_to_delete;
  for (const UserScript& script : scripts) {
    auto map_iter = map.find(script.name());
    // If a content script has the same name as the new one, remove the old
    // script first, and insert the new one.
    if (map_iter != map.end()) {
      scripts_to_delete.insert(map_iter->second);
      map.erase(map_iter);
    }
    map.insert(std::pair<std::string, UserScript>(script.name(), script));
    ids_to_add.insert(script.id());
  }

  if (!scripts_to_delete.empty()) {
    master->RemoveScripts(scripts_to_delete);
  }

  // Step 3: makes WebViewContentScriptManager become an observer of the
  // |loader| for scripts loaded event.
  UserScriptLoader* loader = master->loader();
  DCHECK(loader);
  if (!user_script_loader_observer_.IsObserving(loader))
    user_script_loader_observer_.Add(loader);

  // Step 4: adds new scripts to the master.
  master->AddScripts(scripts, embedder_process_id, embedder_routing_id);

  // Step 5: creates owner web contents observer for the given
  // |embedder_web_contents| if it doesn't exist.
  auto observer_iter =
      owner_web_contents_observer_map_.find(embedder_web_contents);
  if (observer_iter == owner_web_contents_observer_map_.end()) {
    linked_ptr<OwnerWebContentsObserver> observer(
        new OwnerWebContentsObserver(embedder_web_contents, host_id, this));
    observer->add_view_instance_id(view_instance_id);
    owner_web_contents_observer_map_[embedder_web_contents] = observer;
  } else {
    observer_iter->second->add_view_instance_id(view_instance_id);
  }

  // Step 6: updates WebViewRenderState in the IO thread.
  // It is safe to use base::Unretained(WebViewRendererState::GetInstance())
  // since WebViewRendererState::GetInstance() always returns a Singleton of
  // WebViewRendererState.
  if (!ids_to_add.empty()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&WebViewRendererState::AddContentScriptIDs,
                   base::Unretained(WebViewRendererState::GetInstance()),
                   embedder_process_id, view_instance_id, ids_to_add));
  }
}

void WebViewContentScriptManager::RemoveContentScripts(
    content::WebContents* embedder_web_contents,
    int view_instance_id,
    const HostID& host_id,
    const std::vector<std::string>& script_name_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int embedder_process_id =
      embedder_web_contents->GetRenderProcessHost()->GetID();
  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  GuestContentScriptMap::iterator script_map_iter =
      guest_content_script_map_.find(key);
  if (script_map_iter == guest_content_script_map_.end())
    return;

  DeclarativeUserScriptMaster* master =
      DeclarativeUserScriptManager::Get(browser_context_)
          ->GetDeclarativeUserScriptMasterByID(host_id);
  CHECK(master);

  // We need to update WebViewRenderState in the IO thread if the guest exists.
  std::set<int> ids_to_delete;
  std::set<UserScript> scripts_to_delete;

  // Step 1: removes content scripts from |master| and updates
  // |guest_content_script_map_|.
  std::map<std::string, UserScript>& map = script_map_iter->second;
  // If the |script_name_list| is empty, all the content scripts added by the
  // guest will be removed; otherwise, removes the scripts in the
  // |script_name_list|.
  if (script_name_list.empty()) {
    auto it = map.begin();
    while (it != map.end()) {
      scripts_to_delete.insert(it->second);
      ids_to_delete.insert(it->second.id());
      map.erase(it++);
    }
  } else {
    for (const std::string& name : script_name_list) {
      ContentScriptMap::iterator iter = map.find(name);
      if (iter == map.end())
        continue;
      const UserScript& script = iter->second;
      ids_to_delete.insert(script.id());
      scripts_to_delete.insert(script);
      map.erase(iter);
    }
  }

  // Step 2: makes WebViewContentScriptManager become an observer of the
  // |loader| for scripts loaded event.
  UserScriptLoader* loader = master->loader();
  DCHECK(loader);
  if (!user_script_loader_observer_.IsObserving(loader))
    user_script_loader_observer_.Add(loader);

  // Step 3: removes content scripts from master.
  master->RemoveScripts(scripts_to_delete);

  // Step 4: updates WebViewRenderState in the IO thread.
  if (!ids_to_delete.empty()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&WebViewRendererState::RemoveContentScriptIDs,
                   base::Unretained(WebViewRendererState::GetInstance()),
                   embedder_process_id, view_instance_id, ids_to_delete));
  }
}

void WebViewContentScriptManager::RemoveObserver(
    content::WebContents* embedder_web_contents) {
  owner_web_contents_observer_map_.erase(embedder_web_contents);
}

std::set<int> WebViewContentScriptManager::GetContentScriptIDSet(
    int embedder_process_id,
    int view_instance_id) {
  std::set<int> ids;

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  GuestContentScriptMap::const_iterator iter =
      guest_content_script_map_.find(key);
  if (iter == guest_content_script_map_.end())
    return ids;
  const ContentScriptMap& map = iter->second;
  for (const auto& pair : map)
    ids.insert(pair.second.id());

  return ids;
}

void WebViewContentScriptManager::SignalOnScriptsLoaded(
    const base::Closure& callback) {
  if (!user_script_loader_observer_.IsObservingSources()) {
    callback.Run();
    return;
  }
  pending_scripts_loading_callbacks_.push_back(callback);
}

void WebViewContentScriptManager::OnScriptsLoaded(UserScriptLoader* loader) {
  user_script_loader_observer_.Remove(loader);
  RunCallbacksIfReady();
}

void WebViewContentScriptManager::OnUserScriptLoaderDestroyed(
    UserScriptLoader* loader) {
  user_script_loader_observer_.Remove(loader);
  RunCallbacksIfReady();
}

void WebViewContentScriptManager::RunCallbacksIfReady() {
  if (user_script_loader_observer_.IsObservingSources())
    return;
  for (auto& callback : pending_scripts_loading_callbacks_)
    callback.Run();
  pending_scripts_loading_callbacks_.clear();
}

}  // namespace extensions
