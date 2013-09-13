// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/media_galleries_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options {

MediaGalleriesHandler::MediaGalleriesHandler()
    : weak_ptr_factory_(this) {
}

MediaGalleriesHandler::~MediaGalleriesHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
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

void MediaGalleriesHandler::InitializePage() {
  StorageMonitor::GetInstance()->EnsureInitialized(
      base::Bind(&MediaGalleriesHandler::InitializeOnStorageMonitorInit,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MediaGalleriesHandler::InitializeOnStorageMonitorInit() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!MediaGalleriesPreferences::APIHasBeenUsed(profile))
    return;

  if (pref_change_registrar_.IsEmpty()) {
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kMediaGalleriesRememberedGalleries,
        base::Bind(&MediaGalleriesHandler::OnGalleriesChanged,
                   base::Unretained(this)));
  }

  OnGalleriesChanged();
}

void MediaGalleriesHandler::RegisterMessages() {
  StorageMonitor::GetInstance()->EnsureInitialized(
      base::Bind(&MediaGalleriesHandler::RegisterOnStorageMonitorInit,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MediaGalleriesHandler::RegisterOnStorageMonitorInit() {
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
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  Profile* profile = Profile::FromWebUI(web_ui());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);

  ListValue list;
  const MediaGalleriesPrefInfoMap& galleries = preferences->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator iter = galleries.begin();
       iter != galleries.end(); ++iter) {
    const MediaGalleryPrefInfo& gallery = iter->second;
    if (gallery.type == MediaGalleryPrefInfo::kBlackListed)
      continue;

    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("displayName", gallery.GetGalleryDisplayName());
    dict->SetString("path", gallery.AbsolutePath().LossyDisplayName());
    dict->SetString("id", base::Uint64ToString(gallery.pref_id));
    list.Append(dict);
  }

  web_ui()->CallJavascriptFunction(
      "options.MediaGalleriesManager.setAvailableMediaGalleries", list);
}

void MediaGalleriesHandler::HandleAddNewGallery(const base::ListValue* args) {
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      string16(),  // TODO(estade): a name for the dialog?
      base::FilePath(),
      NULL, 0,
      base::FilePath::StringType(),
      web_ui()->GetWebContents()->GetView()->
          GetTopLevelNativeWindow(),
      NULL);
}

void MediaGalleriesHandler::HandleForgetGallery(const base::ListValue* args) {
  std::string string_id;
  uint64 id = 0;
  if (!args->GetString(0, &string_id) ||
      !base::StringToUint64(string_id, &id)) {
    NOTREACHED();
    return;
  }

  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          Profile::FromWebUI(web_ui()));
  preferences->ForgetGalleryById(id);
}

void MediaGalleriesHandler::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          Profile::FromWebUI(web_ui()));
  preferences->AddGalleryByPath(path);
}

}  // namespace options
