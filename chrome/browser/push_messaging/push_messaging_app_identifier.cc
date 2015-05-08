// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"

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

const char kPushMessagingAppIdentifierPrefix[] = "wp:";

// static
void PushMessagingAppIdentifier::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kPushMessagingAppIdentifierMap);
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::Generate(
    const GURL& origin, int64 service_worker_registration_id)
{
  std::string guid = base::GenerateGUID();
  CHECK(!guid.empty());
  std::string app_id = kPushMessagingAppIdentifierPrefix + guid;

  PushMessagingAppIdentifier app_identifier(app_id, origin,
                                            service_worker_registration_id);
  DCHECK(app_identifier.IsValid());
  return app_identifier;
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::Get(
    Profile* profile, const std::string& app_id) {
  // Workaround crbug.com/461867 in GCM where it converts subtypes to lowercase.
  // TODO(johnme): Remove this when obsolete
  const size_t prefix_len = strlen(kPushMessagingAppIdentifierPrefix);
  if (app_id.size() < prefix_len)
    return PushMessagingAppIdentifier();
  std::string uppercase_app_id =
      app_id.substr(0, prefix_len) +
      StringToUpperASCII(app_id.substr(prefix_len, std::string::npos));

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingAppIdentifierMap);

  std::string origin_and_sw_id;
  if (!map->GetStringWithoutPathExpansion(uppercase_app_id, &origin_and_sw_id))
    return PushMessagingAppIdentifier();

  std::vector<std::string> parts;
  base::SplitString(origin_and_sw_id, kSeparator, &parts);
  if (parts.size() != 2)
    return PushMessagingAppIdentifier();

  GURL origin = GURL(parts[0]);

  int64 service_worker_registration_id;
  if (!base::StringToInt64(parts[1], &service_worker_registration_id))
    return PushMessagingAppIdentifier();

  PushMessagingAppIdentifier app_identifier(uppercase_app_id, origin,
                                            service_worker_registration_id);
  DCHECK(app_identifier.IsValid());
  return app_identifier;
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::Get(
    Profile* profile, const GURL& origin, int64 service_worker_registration_id)
{
  base::StringValue origin_and_sw_id = base::StringValue(origin.spec() +
      kSeparator + base::Int64ToString(service_worker_registration_id));

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingAppIdentifierMap);
  for (auto it = base::DictionaryValue::Iterator(*map); !it.IsAtEnd();
       it.Advance()) {
    if (it.value().Equals(&origin_and_sw_id))
      return Get(profile, it.key());
  }
  return PushMessagingAppIdentifier();
}

// static
std::vector<PushMessagingAppIdentifier> PushMessagingAppIdentifier::GetAll(
    Profile* profile) {
  std::vector<PushMessagingAppIdentifier> result;

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingAppIdentifierMap);
  for (auto it = base::DictionaryValue::Iterator(*map); !it.IsAtEnd();
       it.Advance()) {
    result.push_back(Get(profile, it.key()));
  }

  return result;
}

void PushMessagingAppIdentifier::PersistToDisk(Profile* profile) const {
  DCHECK(IsValid());

  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::DictionaryValue* map = update.Get();

  // Delete any stale entry with the same origin and Service Worker
  // registration id (hence we ensure there is a 1:1 not 1:many mapping).
  PushMessagingAppIdentifier old = Get(profile, origin_,
                                       service_worker_registration_id_);
  if (old.IsValid())
    map->RemoveWithoutPathExpansion(old.app_id_, nullptr);

  std::string origin_and_sw_id = origin_.spec() + kSeparator +
      base::Int64ToString(service_worker_registration_id_);
  map->SetStringWithoutPathExpansion(app_id_, origin_and_sw_id);
}

void PushMessagingAppIdentifier::DeleteFromDisk(Profile* profile) const {
  DCHECK(IsValid());

  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::DictionaryValue* map = update.Get();
  map->RemoveWithoutPathExpansion(app_id_, nullptr);
}

PushMessagingAppIdentifier::PushMessagingAppIdentifier()
    : origin_(GURL::EmptyGURL()),
      service_worker_registration_id_(-1) {
}

PushMessagingAppIdentifier::PushMessagingAppIdentifier(
    const std::string& app_id,
    const GURL& origin,
    int64 service_worker_registration_id)
    : app_id_(app_id),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id) {
}

PushMessagingAppIdentifier::~PushMessagingAppIdentifier() {
}

bool PushMessagingAppIdentifier::IsValid() const {
  const size_t prefix_len = strlen(kPushMessagingAppIdentifierPrefix);
  return origin_.is_valid() && origin_.GetOrigin() == origin_
      && service_worker_registration_id_ >= 0
      && !app_id_.compare(0, prefix_len, kPushMessagingAppIdentifierPrefix)
      && base::IsValidGUID(app_id_.substr(prefix_len, std::string::npos));
}
