// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chooser_context_base.h"

#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/origin.h"

const char kObjectListKey[] = "chosen-objects";

ChooserContextBase::ChooserContextBase(
    Profile* profile,
    const ContentSettingsType guard_content_settings_type,
    const ContentSettingsType data_content_settings_type)
    : guard_content_settings_type_(guard_content_settings_type),
      data_content_settings_type_(data_content_settings_type),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)) {
  DCHECK(host_content_settings_map_);
}

ChooserContextBase::~ChooserContextBase() = default;

ChooserContextBase::Object::Object(GURL requesting_origin,
                                   GURL embedding_origin,
                                   base::Value value,
                                   content_settings::SettingSource source,
                                   bool incognito)
    : requesting_origin(requesting_origin),
      embedding_origin(embedding_origin),
      value(std::move(value)),
      source(source),
      incognito(incognito) {}

ChooserContextBase::Object::~Object() = default;

void ChooserContextBase::PermissionObserver::OnChooserObjectPermissionChanged(
    ContentSettingsType data_content_settings_type,
    ContentSettingsType guard_content_settings_type) {}

void ChooserContextBase::PermissionObserver::OnPermissionRevoked(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

void ChooserContextBase::AddObserver(PermissionObserver* observer) {
  permission_observer_list_.AddObserver(observer);
}

void ChooserContextBase::RemoveObserver(PermissionObserver* observer) {
  permission_observer_list_.RemoveObserver(observer);
}

bool ChooserContextBase::CanRequestObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          guard_content_settings_type_, std::string());
  DCHECK(content_setting == CONTENT_SETTING_ASK ||
         content_setting == CONTENT_SETTING_BLOCK);
  return content_setting == CONTENT_SETTING_ASK;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
ChooserContextBase::GetGrantedObjects(const GURL& requesting_origin_url,
                                      const GURL& embedding_origin_url) {
  const auto requesting_origin = url::Origin::Create(requesting_origin_url);
  DCHECK_EQ(requesting_origin_url, requesting_origin.GetURL());
  const auto embedding_origin = url::Origin::Create(embedding_origin_url);
  DCHECK_EQ(embedding_origin_url, embedding_origin.GetURL());

  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return {};

  content_settings::SettingInfo info;
  base::Value setting =
      GetWebsiteSetting(requesting_origin, embedding_origin, &info);

  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return {};

  std::vector<std::unique_ptr<Object>> results;
  for (auto& object : objects->GetList()) {
    if (IsValidObject(object)) {
      results.push_back(std::make_unique<Object>(
          requesting_origin_url, embedding_origin_url, std::move(object),
          info.source, host_content_settings_map_->is_incognito()));
    }
  }
  return results;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
ChooserContextBase::GetAllGrantedObjects() {
  ContentSettingsForOneType content_settings;
  host_content_settings_map_->GetSettingsForOneType(
      data_content_settings_type_, std::string(), &content_settings);

  std::vector<std::unique_ptr<Object>> results;
  for (const ContentSettingPatternSource& content_setting : content_settings) {
    GURL requesting_origin_url(content_setting.primary_pattern.ToString());
    GURL embedding_origin_url(content_setting.secondary_pattern.ToString());
    if (!requesting_origin_url.is_valid() || !embedding_origin_url.is_valid())
      continue;

    const auto requesting_origin = url::Origin::Create(requesting_origin_url);
    const auto embedding_origin = url::Origin::Create(embedding_origin_url);
    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    content_settings::SettingInfo info;
    base::Value setting =
        GetWebsiteSetting(requesting_origin, embedding_origin, &info);
    base::Value* objects = setting.FindListKey(kObjectListKey);
    if (!objects)
      continue;

    for (auto& object : objects->GetList()) {
      if (!IsValidObject(object)) {
        continue;
      }

      results.push_back(std::make_unique<Object>(
          requesting_origin_url, embedding_origin_url, std::move(object),
          info.source, content_setting.incognito));
    }
  }

  return results;
}

void ChooserContextBase::GrantObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    base::Value object) {
  DCHECK(IsValidObject(object));

  base::Value setting =
      GetWebsiteSetting(requesting_origin, embedding_origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects) {
    objects =
        setting.SetKey(kObjectListKey, base::Value(base::Value::Type::LIST));
  }

  auto& object_list = objects->GetList();
  if (!base::ContainsValue(object_list, object))
    object_list.push_back(std::move(object));

  SetWebsiteSetting(requesting_origin, embedding_origin, std::move(setting));
  NotifyPermissionChanged();
}

void ChooserContextBase::RevokeObjectPermission(
    const GURL& requesting_origin_url,
    const GURL& embedding_origin_url,
    const base::Value& object) {
  const auto requesting_origin = url::Origin::Create(requesting_origin_url);
  DCHECK_EQ(requesting_origin_url, requesting_origin.GetURL());
  const auto embedding_origin = url::Origin::Create(embedding_origin_url);
  DCHECK_EQ(embedding_origin_url, embedding_origin.GetURL());
  DCHECK(IsValidObject(object));

  base::Value setting =
      GetWebsiteSetting(requesting_origin, embedding_origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return;

  auto& object_list = objects->GetList();
  auto it = std::find(object_list.begin(), object_list.end(), object);
  if (it != object_list.end())
    objects->GetList().erase(it);

  SetWebsiteSetting(requesting_origin, embedding_origin, std::move(setting));
  NotifyPermissionRevoked(requesting_origin_url, embedding_origin_url);
}

void ChooserContextBase::NotifyPermissionChanged() {
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
  }
}

void ChooserContextBase::NotifyPermissionRevoked(const GURL& requesting_origin,
                                                 const GURL& embedding_origin) {
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    observer.OnPermissionRevoked(requesting_origin, embedding_origin);
  }
}

base::Value ChooserContextBase::GetWebsiteSetting(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    content_settings::SettingInfo* info) {
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          data_content_settings_type_, std::string(), info);
  if (value)
    return base::Value::FromUniquePtrValue(std::move(value));
  return base::Value(base::Value::Type::DICTIONARY);
}

void ChooserContextBase::SetWebsiteSetting(const url::Origin& requesting_origin,
                                           const url::Origin& embedding_origin,
                                           base::Value value) {
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      requesting_origin.GetURL(), embedding_origin.GetURL(),
      data_content_settings_type_, std::string(),
      base::Value::ToUniquePtrValue(std::move(value)));
}
