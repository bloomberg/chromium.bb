// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chooser_context_base.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

using ObjectList = ScopedVector<base::DictionaryValue>;

const char kObjectListKey[] = "chosen-objects";

ChooserContextBase::ChooserContextBase(
    Profile* profile,
    const ContentSettingsType data_content_settings_type)
    : host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)),
      data_content_settings_type_(data_content_settings_type) {
  DCHECK(host_content_settings_map_);
}

ChooserContextBase::~ChooserContextBase() {}

ObjectList ChooserContextBase::GetGrantedObjects(const GURL& requesting_origin,
                                                 const GURL& embedding_origin) {
  scoped_ptr<base::DictionaryValue> setting =
      GetWebsiteSetting(requesting_origin, embedding_origin);
  scoped_ptr<base::Value> objects;
  if (!setting->Remove(kObjectListKey, &objects))
    return ObjectList();

  scoped_ptr<base::ListValue> object_list =
      base::ListValue::From(objects.Pass());
  if (!object_list)
    return ObjectList();

  ObjectList results;
  for (base::ListValue::iterator it = object_list->begin();
       it != object_list->end(); ++it) {
    // Steal ownership of |object| from |object_list|.
    scoped_ptr<base::Value> object(*it);
    *it = nullptr;

    scoped_ptr<base::DictionaryValue> object_dict =
        base::DictionaryValue::From(object.Pass());
    if (object_dict && IsValidObject(*object_dict))
      results.push_back(object_dict.Pass());
  }
  return results.Pass();
}

void ChooserContextBase::GrantObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    scoped_ptr<base::DictionaryValue> object) {
  DCHECK(object && IsValidObject(*object));
  scoped_ptr<base::DictionaryValue> setting =
      GetWebsiteSetting(requesting_origin, embedding_origin);
  base::ListValue* object_list;
  if (!setting->GetList(kObjectListKey, &object_list)) {
    object_list = new base::ListValue();
    setting->Set(kObjectListKey, object_list);
  }
  object_list->AppendIfNotPresent(object.release());
  SetWebsiteSetting(requesting_origin, embedding_origin, setting.Pass());
}

void ChooserContextBase::RevokeObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::DictionaryValue& object) {
  DCHECK(IsValidObject(object));
  scoped_ptr<base::DictionaryValue> setting =
      GetWebsiteSetting(requesting_origin, embedding_origin);
  base::ListValue* object_list;
  if (!setting->GetList(kObjectListKey, &object_list))
    return;
  object_list->Remove(object, nullptr);
  SetWebsiteSetting(requesting_origin, embedding_origin, setting.Pass());
}

scoped_ptr<base::DictionaryValue> ChooserContextBase::GetWebsiteSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  scoped_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(host_content_settings_map_->GetWebsiteSetting(
          requesting_origin, embedding_origin, data_content_settings_type_,
          std::string(), nullptr));
  if (!value)
    value.reset(new base::DictionaryValue());

  return value.Pass();
}

void ChooserContextBase::SetWebsiteSetting(const GURL& requesting_origin,
                                           const GURL& embedding_origin,
                                           scoped_ptr<base::Value> value) {
  ContentSettingsPattern primary_pattern(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin));
  ContentSettingsPattern secondary_pattern(
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin));
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  host_content_settings_map_->SetWebsiteSetting(
      primary_pattern, secondary_pattern, data_content_settings_type_,
      std::string(), value.release());
}
