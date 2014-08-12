// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/extension_information.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_base.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;
using extensions::Extension;

namespace {

const Extension* GetExtensionForWebContents(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extensions::ProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  return process_manager->GetExtensionForRenderViewHost(
      web_contents->GetRenderViewHost());
}

}  // namespace

namespace task_manager {

class ExtensionProcessResource : public RendererResource {
 public:
  explicit ExtensionProcessResource(const Extension* extension,
                                    content::RenderViewHost* render_view_host);
  virtual ~ExtensionProcessResource();

  // Resource methods:
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;

 private:
  // Returns true if the associated extension has a background page.
  bool IsBackground() const;

  // The icon painted for the extension process.
  static gfx::ImageSkia* default_icon_;

  // Cached data about the extension.
  base::string16 title_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessResource);
};

gfx::ImageSkia* ExtensionProcessResource::default_icon_ = NULL;

ExtensionProcessResource::ExtensionProcessResource(
    const Extension* extension,
    content::RenderViewHost* render_view_host)
    : RendererResource(render_view_host->GetProcess()->GetHandle(),
                       render_view_host) {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
  }
  base::string16 extension_name = base::UTF8ToUTF16(extension->name());
  DCHECK(!extension_name.empty());

  Profile* profile = Profile::FromBrowserContext(
      render_view_host->GetProcess()->GetBrowserContext());
  int message_id = util::GetMessagePrefixID(extension->is_app(),
                                            true,  // is_extension
                                            profile->IsOffTheRecord(),
                                            false,  // is_prerender
                                            IsBackground());
  title_ = l10n_util::GetStringFUTF16(message_id, extension_name);
}

ExtensionProcessResource::~ExtensionProcessResource() {}

base::string16 ExtensionProcessResource::GetTitle() const { return title_; }

gfx::ImageSkia ExtensionProcessResource::GetIcon() const {
  return *default_icon_;
}

Resource::Type ExtensionProcessResource::GetType() const { return EXTENSION; }

bool ExtensionProcessResource::IsBackground() const {
  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host());
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionInformation class
////////////////////////////////////////////////////////////////////////////////

ExtensionInformation::ExtensionInformation() {}

ExtensionInformation::~ExtensionInformation() {}

void ExtensionInformation::GetAll(const NewWebContentsCallback& callback) {
  // Add all the existing extension views from all Profiles, including those
  // from incognito split mode.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  size_t num_default_profiles = profiles.size();
  for (size_t i = 0; i < num_default_profiles; ++i) {
    if (profiles[i]->HasOffTheRecordProfile()) {
      profiles.push_back(profiles[i]->GetOffTheRecordProfile());
    }
  }

  for (size_t i = 0; i < profiles.size(); ++i) {
    extensions::ProcessManager* process_manager =
        extensions::ExtensionSystem::Get(profiles[i])->process_manager();
    if (process_manager) {
      const extensions::ProcessManager::ViewSet all_views =
          process_manager->GetAllViews();
      extensions::ProcessManager::ViewSet::const_iterator jt =
          all_views.begin();
      for (; jt != all_views.end(); ++jt) {
        WebContents* web_contents = WebContents::FromRenderViewHost(*jt);
        if (CheckOwnership(web_contents))
          callback.Run(web_contents);
      }
    }
  }
}

bool ExtensionInformation::CheckOwnership(WebContents* web_contents) {
  // Don't add WebContents that belong to a guest (those are handled by
  // GuestInformation). Otherwise they will be added twice.
  if (extensions::GuestViewBase::IsGuest(web_contents))
    return false;

  // Extension WebContents tracked by this class will always host extension
  // content.
  if (!GetExtensionForWebContents(web_contents))
    return false;

  extensions::ViewType view_type = extensions::GetViewType(web_contents);

  // Don't add tab contents (those are handled by TabContentsResourceProvider)
  // or background contents (handled by BackgroundInformation) or panels
  // (handled by PanelInformation)
  return (view_type != extensions::VIEW_TYPE_INVALID &&
          view_type != extensions::VIEW_TYPE_TAB_CONTENTS &&
          view_type != extensions::VIEW_TYPE_BACKGROUND_CONTENTS &&
          view_type != extensions::VIEW_TYPE_PANEL);
}

scoped_ptr<RendererResource> ExtensionInformation::MakeResource(
    WebContents* web_contents) {
  const Extension* extension = GetExtensionForWebContents(web_contents);
  if (!extension) {
    NOTREACHED();
    return scoped_ptr<RendererResource>();
  }
  return scoped_ptr<RendererResource>(new ExtensionProcessResource(
      extension, web_contents->GetRenderViewHost()));
}

}  // namespace task_manager
