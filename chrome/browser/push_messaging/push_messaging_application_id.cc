// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_application_id.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {
const char kSeparator = '#';  // Ok as only the origin of the url is used.
}  // namespace

const char kPushMessagingApplicationIdPrefix[] = "wp:";

// static
void PushMessagingApplicationId::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kPushMessagingApplicationIdMap,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
PushMessagingApplicationId PushMessagingApplicationId::Generate(
    const GURL& origin, int64 service_worker_registration_id)
{
  // TODO(johnme): Does GenerateGUID produce good enough random numbers?
  std::string guid = base::GenerateGUID();
  CHECK(!guid.empty());
  std::string app_id_guid =
      kPushMessagingApplicationIdPrefix + guid;

  PushMessagingApplicationId application_id(app_id_guid, origin,
                                            service_worker_registration_id);
  DCHECK(application_id.IsValid());
  return application_id;
}

// static
PushMessagingApplicationId PushMessagingApplicationId::Get(
    Profile* profile, const std::string& app_id_guid) {
  // Workaround crbug.com/461867 in GCM where it converts subtypes to lowercase.
  // TODO(johnme): Remove this when obsolete
  const size_t prefix_len = strlen(kPushMessagingApplicationIdPrefix);
  if (app_id_guid.size() < prefix_len)
    return PushMessagingApplicationId();
  std::string uppercase_app_id =
      app_id_guid.substr(0, prefix_len) +
      StringToUpperASCII(app_id_guid.substr(prefix_len, std::string::npos));

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingApplicationIdMap);

  std::string origin_and_sw_id;
  if (!map->GetStringWithoutPathExpansion(uppercase_app_id, &origin_and_sw_id))
    return PushMessagingApplicationId();

  std::vector<std::string> parts;
  base::SplitString(origin_and_sw_id, kSeparator, &parts);
  if (parts.size() != 2)
    return PushMessagingApplicationId();

  GURL origin = GURL(parts[0]);

  int64 service_worker_registration_id;
  if (!base::StringToInt64(parts[1], &service_worker_registration_id))
    return PushMessagingApplicationId();

  PushMessagingApplicationId application_id(uppercase_app_id, origin,
                                            service_worker_registration_id);
  DCHECK(application_id.IsValid());
  return application_id;
}

// static
PushMessagingApplicationId PushMessagingApplicationId::Get(
    Profile* profile, const GURL& origin, int64 service_worker_registration_id)
{
  base::StringValue origin_and_sw_id = base::StringValue(origin.spec() +
      kSeparator + base::Int64ToString(service_worker_registration_id));

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingApplicationIdMap);
  for (auto it = base::DictionaryValue::Iterator(*map); !it.IsAtEnd();
       it.Advance()) {
    if (it.value().Equals(&origin_and_sw_id))
      return Get(profile, it.key());
  }
  return PushMessagingApplicationId();
}

// static
std::vector<PushMessagingApplicationId> PushMessagingApplicationId::GetAll(
    Profile* profile) {
  std::vector<PushMessagingApplicationId> result;

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingApplicationIdMap);
  for (auto it = base::DictionaryValue::Iterator(*map); !it.IsAtEnd();
       it.Advance()) {
    result.push_back(Get(profile, it.key()));
  }

  return result;
}

void PushMessagingApplicationId::PersistToDisk(Profile* profile) const {
  DCHECK(IsValid());

  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingApplicationIdMap);
  base::DictionaryValue* map = update.Get();

  // Delete any stale entry with the same origin and Service Worker
  // registration id (hence we ensure there is a 1:1 not 1:many mapping).
  PushMessagingApplicationId old = Get(profile, origin_,
                                       service_worker_registration_id_);
  if (old.IsValid())
    map->RemoveWithoutPathExpansion(old.app_id_guid_, nullptr);

  std::string origin_and_sw_id = origin_.spec() + kSeparator +
      base::Int64ToString(service_worker_registration_id_);
  map->SetStringWithoutPathExpansion(app_id_guid_, origin_and_sw_id);
}

void PushMessagingApplicationId::DeleteFromDisk(Profile* profile) const {
  DCHECK(IsValid());

  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingApplicationIdMap);
  base::DictionaryValue* map = update.Get();
  map->RemoveWithoutPathExpansion(app_id_guid_, nullptr);
}

PushMessagingApplicationId::PushMessagingApplicationId()
    : origin_(GURL::EmptyGURL()),
      service_worker_registration_id_(-1) {
}

PushMessagingApplicationId::PushMessagingApplicationId(
    const std::string& app_id_guid,
    const GURL& origin,
    int64 service_worker_registration_id)
    : app_id_guid_(app_id_guid),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id) {
}

PushMessagingApplicationId::~PushMessagingApplicationId() {
}

bool PushMessagingApplicationId::IsValid() const {
  const size_t prefix_len = strlen(kPushMessagingApplicationIdPrefix);
  return origin_.is_valid() && origin_.GetOrigin() == origin_
      && service_worker_registration_id_ >= 0
      && !app_id_guid_.compare(0, prefix_len, kPushMessagingApplicationIdPrefix)
      && base::IsValidGUID(app_id_guid_.substr(prefix_len, std::string::npos));
}
