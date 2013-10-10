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
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
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

MediaGalleriesHandler::MediaGalleriesHandler() : weak_ptr_factory_(this) {}

MediaGalleriesHandler::~MediaGalleriesHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  Profile* profile = Profile::FromWebUI(web_ui());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  preferences->RemoveGalleryChangeObserver(this);
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
  Profile* profile = Profile::FromWebUI(web_ui());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesHandler::InitializeHandlerOnMediaGalleriesPreferencesInit,
      weak_ptr_factory_.GetWeakPtr()));
}

void MediaGalleriesHandler::InitializePage() {
  Profile* profile = Profile::FromWebUI(web_ui());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesHandler::InitializePageOnMediaGalleriesPreferencesInit,
      weak_ptr_factory_.GetWeakPtr()));
}

void MediaGalleriesHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesHandler::RegisterOnPreferencesInit,
      weak_ptr_factory_.GetWeakPtr()));
}

void MediaGalleriesHandler::RegisterOnPreferencesInit() {
  web_ui()->RegisterMessageCallback(
      "addNewGallery",
      base::Bind(&MediaGalleriesHandler::HandleAddNewGallery,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "forgetGallery",
      base::Bind(&MediaGalleriesHandler::HandleForgetGallery,
                 base::Unretained(this)));
}

void MediaGalleriesHandler::OnGalleryAdded(MediaGalleriesPreferences* pref,
                                           MediaGalleryPrefId /*pref_id*/) {
  OnGalleriesChanged(pref);
}

void MediaGalleriesHandler::OnGalleryRemoved(MediaGalleriesPreferences* pref,
                                             MediaGalleryPrefId /*pref_id*/) {
  OnGalleriesChanged(pref);
}

void MediaGalleriesHandler::OnGalleryInfoUpdated(
    MediaGalleriesPreferences* pref,
    MediaGalleryPrefId /*pref_id*/) {
  OnGalleriesChanged(pref);
}

void MediaGalleriesHandler::OnGalleriesChanged(
    MediaGalleriesPreferences* pref) {
  DCHECK(pref->IsInitialized());
  ListValue list;
  const MediaGalleriesPrefInfoMap& galleries = pref->known_galleries();
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

void MediaGalleriesHandler::InitializeHandlerOnMediaGalleriesPreferencesInit() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!MediaGalleriesPreferences::APIHasBeenUsed(profile))
    return;
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);

  preferences->AddGalleryChangeObserver(this);
}

void MediaGalleriesHandler::InitializePageOnMediaGalleriesPreferencesInit() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!MediaGalleriesPreferences::APIHasBeenUsed(profile))
    return;
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);

  OnGalleriesChanged(preferences);
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

  media_galleries::UsageCount(media_galleries::WEBUI_FORGET_GALLERY);
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          Profile::FromWebUI(web_ui()));
  preferences->ForgetGalleryById(id);
}

void MediaGalleriesHandler::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  media_galleries::UsageCount(media_galleries::WEBUI_ADD_GALLERY);
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          Profile::FromWebUI(web_ui()));
  preferences->AddGalleryByPath(path);
}

}  // namespace options
