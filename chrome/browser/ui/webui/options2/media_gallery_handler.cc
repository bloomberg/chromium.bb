// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/media_gallery_handler.h"

#include "base/bind.h"
#include "chrome/browser/media_gallery/media_gallery_registry.h"
#include "chrome/browser/media_gallery/media_gallery_registry_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options2 {

MediaGalleryHandler::MediaGalleryHandler() {
}

MediaGalleryHandler::~MediaGalleryHandler() {
}

void MediaGalleryHandler::GetLocalizedValues(DictionaryValue* values) {
  DCHECK(values);

  static OptionsStringResource resources[] = {
    { "mediaGalleriesSectionLabel", IDS_MEDIA_GALLERY_SECTION_LABEL },
    { "manageGalleriesButton", IDS_MEDIA_GALLERY_MANAGE_BUTTON },
    { "addNewGalleryButton", IDS_MEDIA_GALLERY_ADD_NEW_BUTTON },
  };

  RegisterStrings(values, resources, arraysize(resources));
  RegisterTitle(values, "manageMediaGalleries",
                IDS_MEDIA_GALLERY_MANAGE_TITLE);
}

void MediaGalleryHandler::InitializeHandler() {
  if (!MediaGalleryRegistry::UserInteractionIsEnabled())
    return;

  Profile* profile = Profile::FromWebUI(web_ui());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kMediaGalleryRememberedGalleries, this);
}

void MediaGalleryHandler::InitializePage() {
  if (!MediaGalleryRegistry::UserInteractionIsEnabled())
    return;

  OnGalleriesChanged();
}

void MediaGalleryHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "addNewGallery",
      base::Bind(&MediaGalleryHandler::HandleAddNewGallery,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "forgetGallery",
      base::Bind(&MediaGalleryHandler::HandleForgetGallery,
                 base::Unretained(this)));
}

void MediaGalleryHandler::OnGalleriesChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const ListValue* list = profile->GetPrefs()->GetList(
      prefs::kMediaGalleryRememberedGalleries);
  web_ui()->CallJavascriptFunction(
      "options.MediaGalleryManager.setAvailableMediaGalleries", *list);
}

void MediaGalleryHandler::HandleAddNewGallery(const base::ListValue* args) {
  SelectFileDialog* dialog = SelectFileDialog::Create(
      this,
      new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  dialog->SelectFile(SelectFileDialog::SELECT_FOLDER,
                     string16(),  // TODO(estade): a name for the dialog?
                     FilePath(),
                     NULL, 0,
                     FilePath::StringType(),
                     web_ui()->GetWebContents()->GetView()->
                         GetTopLevelNativeWindow(),
                     NULL);
}

void MediaGalleryHandler::HandleForgetGallery(const base::ListValue* args) {
  // TODO(estade): use uint64.
  int id;
  CHECK(ExtractIntegerValue(args, &id));
  MediaGalleryRegistry* registry =
      MediaGalleryRegistryFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  registry->ForgetGalleryById(id);
}

void MediaGalleryHandler::FileSelected(
    const FilePath& path, int index, void* params) {
  MediaGalleryRegistry* registry =
      MediaGalleryRegistryFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  registry->AddGalleryByPath(path);
}

void MediaGalleryHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED &&
      *content::Details<std::string>(details).ptr() ==
          prefs::kMediaGalleryRememberedGalleries) {
    OnGalleriesChanged();
  } else {
    NOTREACHED();
  }
}

}  // namespace options2
