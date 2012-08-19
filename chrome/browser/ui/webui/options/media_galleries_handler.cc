// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/media_galleries_handler.h"

#include "base/bind.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
#include "chrome/browser/media_gallery/media_galleries_preferences_factory.h"
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

namespace options {

MediaGalleriesHandler::MediaGalleriesHandler() {
}

MediaGalleriesHandler::~MediaGalleriesHandler() {
}

void MediaGalleriesHandler::GetLocalizedValues(DictionaryValue* values) {
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

void MediaGalleriesHandler::InitializeHandler() {
  if (!chrome::MediaGalleriesPreferences::UserInteractionIsEnabled())
    return;

  Profile* profile = Profile::FromWebUI(web_ui());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kMediaGalleriesRememberedGalleries, this);
}

void MediaGalleriesHandler::InitializePage() {
  if (!chrome::MediaGalleriesPreferences::UserInteractionIsEnabled())
    return;

  OnGalleriesChanged();
}

void MediaGalleriesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "addNewGallery",
      base::Bind(&MediaGalleriesHandler::HandleAddNewGallery,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "forgetGallery",
      base::Bind(&MediaGalleriesHandler::HandleForgetGallery,
                 base::Unretained(this)));
}

void MediaGalleriesHandler::OnGalleriesChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const ListValue* list = profile->GetPrefs()->GetList(
      prefs::kMediaGalleriesRememberedGalleries);
  web_ui()->CallJavascriptFunction(
      "options.MediaGalleriesManager.setAvailableMediaGalleries", *list);
}

void MediaGalleriesHandler::HandleAddNewGallery(const base::ListValue* args) {
  ui::SelectFileDialog* dialog = ui::SelectFileDialog::Create(
      this,
      new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  dialog->SelectFile(ui::SelectFileDialog::SELECT_FOLDER,
                     string16(),  // TODO(estade): a name for the dialog?
                     FilePath(),
                     NULL, 0,
                     FilePath::StringType(),
                     web_ui()->GetWebContents()->GetView()->
                         GetTopLevelNativeWindow(),
                     NULL);
}

void MediaGalleriesHandler::HandleForgetGallery(const base::ListValue* args) {
  // TODO(estade): use uint64.
  int id;
  CHECK(ExtractIntegerValue(args, &id));
  chrome::MediaGalleriesPreferences* prefs =
      MediaGalleriesPreferencesFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  prefs->ForgetGalleryById(id);
}

void MediaGalleriesHandler::FileSelected(
    const FilePath& path, int index, void* params) {
    chrome::MediaGalleriesPreferences* prefs =
      MediaGalleriesPreferencesFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  prefs->AddGalleryByPath(path);
}

void MediaGalleriesHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED &&
      *content::Details<std::string>(details).ptr() ==
          prefs::kMediaGalleriesRememberedGalleries) {
    OnGalleriesChanged();
  } else {
    NOTREACHED();
  }
}

}  // namespace options
