// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/background_information.h"

#include <stddef.h>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::WebContents;
using extensions::Extension;

namespace task_manager {

class BackgroundContentsResource : public RendererResource {
 public:
  BackgroundContentsResource(BackgroundContents* background_contents,
                             const base::string16& application_name);
  ~BackgroundContentsResource() override;

  // Resource methods:
  base::string16 GetTitle() const override;
  gfx::ImageSkia GetIcon() const override;

  const base::string16& application_name() const { return application_name_; }

 private:
  BackgroundContents* background_contents_;

  base::string16 application_name_;

  // The icon painted for BackgroundContents.
  // TODO(atwilson): Use the favicon when there's a way to get the favicon for
  // BackgroundContents.
  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsResource);
};

gfx::ImageSkia* BackgroundContentsResource::default_icon_ = NULL;

BackgroundContentsResource::BackgroundContentsResource(
    BackgroundContents* background_contents,
    const base::string16& application_name)
    : RendererResource(
          background_contents->web_contents()->GetRenderProcessHost()->
              GetHandle(),
          background_contents->web_contents()->GetRenderViewHost()),
      background_contents_(background_contents),
      application_name_(application_name) {
  // Just use the same icon that other extension resources do.
  // TODO(atwilson): Use the favicon when that's available.
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
  }
  // Ensure that the string has the appropriate direction markers (see comment
  // in TabContentsResource::GetTitle()).
  base::i18n::AdjustStringForLocaleDirection(&application_name_);
}

BackgroundContentsResource::~BackgroundContentsResource() {}

base::string16 BackgroundContentsResource::GetTitle() const {
  base::string16 title = application_name_;

  if (title.empty()) {
    // No title (can't locate the parent app for some reason) so just display
    // the URL (properly forced to be LTR).
    title = base::i18n::GetDisplayStringInLTRDirectionality(
        base::UTF8ToUTF16(background_contents_->GetURL().spec()));
  }
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_PREFIX, title);
}

gfx::ImageSkia BackgroundContentsResource::GetIcon() const {
  return *default_icon_;
}

////////////////////////////////////////////////////////////////////////////////
// BackgroundInformation class
////////////////////////////////////////////////////////////////////////////////

BackgroundInformation::BackgroundInformation() {}

BackgroundInformation::~BackgroundInformation() {}

bool BackgroundInformation::CheckOwnership(WebContents* web_contents) {
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == extensions::VIEW_TYPE_BACKGROUND_CONTENTS;
}

void BackgroundInformation::GetAll(const NewWebContentsCallback& callback) {
  // Add all the existing BackgroundContents from every profile, including
  // incognito profiles.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  size_t num_default_profiles = profiles.size();
  for (size_t i = 0; i < num_default_profiles; ++i) {
    if (profiles[i]->HasOffTheRecordProfile()) {
      profiles.push_back(profiles[i]->GetOffTheRecordProfile());
    }
  }
  for (size_t i = 0; i < profiles.size(); ++i) {
    BackgroundContentsService* background_contents_service =
        BackgroundContentsServiceFactory::GetForProfile(profiles[i]);
    std::vector<BackgroundContents*> contents =
        background_contents_service->GetBackgroundContents();
    for (std::vector<BackgroundContents*>::iterator iterator = contents.begin();
         iterator != contents.end();
         ++iterator) {
      callback.Run((*iterator)->web_contents());
    }
  }
}

std::unique_ptr<RendererResource> BackgroundInformation::MakeResource(
    WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const extensions::ExtensionSet& extensions_set =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  BackgroundContentsService* background_contents_service =
      BackgroundContentsServiceFactory::GetForProfile(profile);
  std::vector<BackgroundContents*> contents =
      background_contents_service->GetBackgroundContents();
  for (std::vector<BackgroundContents*>::iterator iterator = contents.begin();
       iterator != contents.end();
       ++iterator) {
    if ((*iterator)->web_contents() == web_contents) {
      base::string16 application_name;
      // Lookup the name from the parent extension.
      const base::string16& application_id =
          background_contents_service->GetParentApplicationId(*iterator);
      const Extension* extension =
          extensions_set.GetByID(base::UTF16ToUTF8(application_id));
      if (extension)
        application_name = base::UTF8ToUTF16(extension->name());
      return std::unique_ptr<RendererResource>(
          new BackgroundContentsResource(*iterator, application_name));
    }
  }
  NOTREACHED();
  return std::unique_ptr<RendererResource>();
}

}  // namespace task_manager
