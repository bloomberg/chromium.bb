// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/tab_contents_information.h"

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;

namespace task_manager {

namespace {

// Returns true if the WebContents is currently owned by the prerendering
// manager.
bool IsContentsPrerendering(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  return prerender_manager &&
         prerender_manager->IsWebContentsPrerendering(web_contents, NULL);
}

}  // namespace

// Tracks a single tab contents, prerendered page, or Instant page.
class TabContentsResource : public RendererResource {
 public:
  explicit TabContentsResource(content::WebContents* web_contents);
  virtual ~TabContentsResource();

  // Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  // Returns true if contains content rendered by an extension.
  bool HostsExtension() const;

  static gfx::ImageSkia* prerender_icon_;
  content::WebContents* web_contents_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsResource);
};

gfx::ImageSkia* TabContentsResource::prerender_icon_ = NULL;

TabContentsResource::TabContentsResource(WebContents* web_contents)
    : RendererResource(web_contents->GetRenderProcessHost()->GetHandle(),
                       web_contents->GetRenderViewHost()),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  if (!prerender_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    prerender_icon_ = rb.GetImageSkiaNamed(IDR_PRERENDER);
  }
}

TabContentsResource::~TabContentsResource() {}

bool TabContentsResource::HostsExtension() const {
  return web_contents_->GetURL().SchemeIs(extensions::kExtensionScheme);
}

Resource::Type TabContentsResource::GetType() const {
  // A tab that loads an extension URL is considered to be an extension for
  // these purposes, although it's tracked as a TabContentsResource.
  return HostsExtension() ? EXTENSION : RENDERER;
}

base::string16 TabContentsResource::GetTitle() const {
  // Fall back on the URL if there's no title.
  GURL url = web_contents_->GetURL();
  base::string16 tab_title = util::GetTitleFromWebContents(web_contents_);

  // Only classify as an app if the URL is an app and the tab is hosting an
  // extension process.  (It's possible to be showing the URL from before it
  // was installed as an app.)
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile_);
  bool is_app =
      extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions().GetAppByURL(url) != NULL &&
      process_map->Contains(web_contents_->GetRenderProcessHost()->GetID());

  int message_id = util::GetMessagePrefixID(
      is_app,
      HostsExtension(),
      profile_->IsOffTheRecord(),
      IsContentsPrerendering(web_contents_),
      false);  // is_background
  return l10n_util::GetStringFUTF16(message_id, tab_title);
}

gfx::ImageSkia TabContentsResource::GetIcon() const {
  if (IsContentsPrerendering(web_contents_))
    return *prerender_icon_;
  FaviconTabHelper::CreateForWebContents(web_contents_);
  return FaviconTabHelper::FromWebContents(web_contents_)->
      GetFavicon().AsImageSkia();
}

WebContents* TabContentsResource::GetWebContents() const {
  return web_contents_;
}

TabContentsInformation::TabContentsInformation() {}

TabContentsInformation::~TabContentsInformation() {}

bool TabContentsInformation::CheckOwnership(
    content::WebContents* web_contents) {
  return chrome::FindBrowserWithWebContents(web_contents) ||
         DevToolsWindow::IsDevToolsWindow(web_contents) ||
         IsContentsPrerendering(web_contents);
}

void TabContentsInformation::GetAll(const NewWebContentsCallback& callback) {
  for (TabContentsIterator iterator; !iterator.done(); iterator.Next()) {
    callback.Run(*iterator);
    WebContents* devtools =
        DevToolsWindow::GetInTabWebContents(*iterator, NULL);
    if (devtools)
      callback.Run(devtools);
  }

  // Because a WebContents* may start its life as a prerender, and later be
  // put into a tab, this class tracks the prerender contents in the same
  // way as the tab contents.
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profiles[i]);
    if (prerender_manager) {
      const std::vector<content::WebContents*> contentses =
          prerender_manager->GetAllPrerenderingContents();
      for (size_t j = 0; j < contentses.size(); ++j)
        callback.Run(contentses[j]);
    }
  }
}

scoped_ptr<RendererResource> TabContentsInformation::MakeResource(
    content::WebContents* web_contents) {
  return scoped_ptr<RendererResource>(new TabContentsResource(web_contents));
}

}  // namespace task_manager
