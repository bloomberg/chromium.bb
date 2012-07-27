// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_galleries_preferences.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace {

const char kMediaGalleriesIdKey[] = "id";
const char kMediaGalleriesPathKey[] = "path";
const char kMediaGalleriesDisplayNameKey[] = "displayName";

bool GetId(const DictionaryValue* dict, uint64* value) {
  std::string string_id;
  if (!dict->GetString(kMediaGalleriesIdKey, &string_id) ||
      !base::StringToUint64(string_id, value)) {
    return false;
  }

  return true;
}

bool PopulateGalleryFromDictionary(const DictionaryValue* dict,
                                   MediaGallery* out_gallery) {
  uint64 id;
  FilePath::StringType path;
  string16 display_name;
  if (!GetId(dict, &id) ||
      !dict->GetString(kMediaGalleriesPathKey, &path) ||
      !dict->GetString(kMediaGalleriesDisplayNameKey, &display_name)) {
    return false;
  }

  out_gallery->id = id;
  out_gallery->path = FilePath(path);
  out_gallery->display_name = display_name;
  return true;
}

DictionaryValue* CreateGalleryDictionary(const MediaGallery& gallery) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kMediaGalleriesIdKey, base::Uint64ToString(gallery.id));
  dict->SetString(kMediaGalleriesPathKey, gallery.path.value());
  // TODO(estade): save |path| and |identifier|.
  dict->SetString(kMediaGalleriesDisplayNameKey, gallery.display_name);
  return dict;
}

}  // namespace

MediaGallery::MediaGallery() : id(0) {}
MediaGallery::~MediaGallery() {}

MediaGalleriesPreferences::MediaGalleriesPreferences(Profile* profile)
    : profile_(profile) {
  DCHECK(UserInteractionIsEnabled());
  InitFromPrefs();
}

MediaGalleriesPreferences::~MediaGalleriesPreferences() {}

void MediaGalleriesPreferences::InitFromPrefs() {
  remembered_galleries_.clear();

  PrefService* prefs = profile_->GetPrefs();
  const ListValue* list = prefs->GetList(
      prefs::kMediaGalleriesRememberedGalleries);

  for (ListValue::const_iterator it = list->begin(); it != list->end(); it++) {
    const DictionaryValue* dict = NULL;
    if (!(*it)->GetAsDictionary(&dict))
      continue;

    MediaGallery gallery;
    if (PopulateGalleryFromDictionary(dict, &gallery))
      remembered_galleries_.push_back(gallery);
  }
}

void MediaGalleriesPreferences::AddGalleryByPath(const FilePath& path) {
  PrefService* prefs = profile_->GetPrefs();

  MediaGallery gallery;
  gallery.id = prefs->GetUint64(prefs::kMediaGalleriesUniqueId);
  prefs->SetUint64(prefs::kMediaGalleriesUniqueId, gallery.id + 1);
  gallery.display_name = path.BaseName().LossyDisplayName();
  // TODO(estade): make this relative to base_path.
  gallery.path = path;
  // TODO(estade): populate the rest of the fields.

  ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
  ListValue* list = update.Get();
  list->Append(CreateGalleryDictionary(gallery));

  remembered_galleries_.push_back(gallery);
}

void MediaGalleriesPreferences::ForgetGalleryById(uint64 id) {
  PrefService* prefs = profile_->GetPrefs();
  ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
  ListValue* list = update.Get();

  for (ListValue::iterator iter = list->begin(); iter != list->end(); ++iter) {
    DictionaryValue* dict;
    uint64 iter_id;
    if ((*iter)->GetAsDictionary(&dict) && GetId(dict, &iter_id) &&
        id == iter_id) {
      list->Erase(iter, NULL);
      InitFromPrefs();
      return;
    }
  }
}

void MediaGalleriesPreferences::Shutdown() {
  profile_ = NULL;
}

// static
bool MediaGalleriesPreferences::UserInteractionIsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMediaGalleryUI);
}

// static
void MediaGalleriesPreferences::RegisterUserPrefs(PrefService* prefs) {
  if (!UserInteractionIsEnabled())
    return;

  prefs->RegisterListPref(prefs::kMediaGalleriesRememberedGalleries,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterUint64Pref(prefs::kMediaGalleriesUniqueId, 0,
                            PrefService::UNSYNCABLE_PREF);
}
