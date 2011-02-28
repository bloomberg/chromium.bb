// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type.h"

#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"

namespace syncable {

void AddDefaultExtensionValue(syncable::ModelType datatype,
                              sync_pb::EntitySpecifics* specifics) {
  switch (datatype) {
    case BOOKMARKS:
      specifics->MutableExtension(sync_pb::bookmark);
      break;
    case PASSWORDS:
      specifics->MutableExtension(sync_pb::password);
      break;
    case PREFERENCES:
      specifics->MutableExtension(sync_pb::preference);
      break;
    case AUTOFILL:
      specifics->MutableExtension(sync_pb::autofill);
      break;
    case AUTOFILL_PROFILE:
      specifics->MutableExtension(sync_pb::autofill_profile);
      break;
    case THEMES:
      specifics->MutableExtension(sync_pb::theme);
      break;
    case TYPED_URLS:
      specifics->MutableExtension(sync_pb::typed_url);
      break;
    case EXTENSIONS:
      specifics->MutableExtension(sync_pb::extension);
      break;
    case NIGORI:
      specifics->MutableExtension(sync_pb::nigori);
      break;
    case SESSIONS:
      specifics->MutableExtension(sync_pb::session);
      break;
    case APPS:
      specifics->MutableExtension(sync_pb::app);
      break;
    default:
      NOTREACHED() << "No known extension for model type.";
  }
}

ModelType GetModelTypeFromExtensionFieldNumber(int field_number) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    ModelType model_type = ModelTypeFromInt(i);
    if (GetExtensionFieldNumberFromModelType(model_type) == field_number)
      return model_type;
  }
  NOTREACHED();
  return UNSPECIFIED;
}

int GetExtensionFieldNumberFromModelType(ModelType model_type) {
  switch (model_type) {
    case BOOKMARKS:
      return sync_pb::kBookmarkFieldNumber;
      break;
    case PASSWORDS:
      return sync_pb::kPasswordFieldNumber;
      break;
    case PREFERENCES:
      return sync_pb::kPreferenceFieldNumber;
      break;
    case AUTOFILL:
      return sync_pb::kAutofillFieldNumber;
      break;
    case AUTOFILL_PROFILE:
      return sync_pb::kAutofillProfileFieldNumber;
      break;
    case THEMES:
      return sync_pb::kThemeFieldNumber;
      break;
    case TYPED_URLS:
      return sync_pb::kTypedUrlFieldNumber;
      break;
    case EXTENSIONS:
      return sync_pb::kExtensionFieldNumber;
      break;
    case NIGORI:
      return sync_pb::kNigoriFieldNumber;
      break;
    case SESSIONS:
      return sync_pb::kSessionFieldNumber;
      break;
    case APPS:
      return sync_pb::kAppFieldNumber;
      break;
    default:
      NOTREACHED() << "No known extension for model type.";
      return 0;
  }
  NOTREACHED() << "Needed for linux_keep_shadow_stacks because of "
               << "http://gcc.gnu.org/bugzilla/show_bug.cgi?id=20681";
  return 0;
}

// Note: keep this consistent with GetModelType in syncable.cc!
ModelType GetModelType(const sync_pb::SyncEntity& sync_pb_entity) {
  const browser_sync::SyncEntity& sync_entity =
      static_cast<const browser_sync::SyncEntity&>(sync_pb_entity);
  DCHECK(!sync_entity.id().IsRoot());  // Root shouldn't ever go over the wire.

  if (sync_entity.deleted())
    return UNSPECIFIED;

  // Backwards compatibility with old (pre-specifics) protocol.
  if (sync_entity.has_bookmarkdata())
    return BOOKMARKS;

  ModelType specifics_type = GetModelTypeFromSpecifics(sync_entity.specifics());
  if (specifics_type != UNSPECIFIED)
    return specifics_type;

  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!sync_entity.server_defined_unique_tag().empty() &&
      sync_entity.IsFolder()) {
    return TOP_LEVEL_FOLDER;
  }

  // This is an item of a datatype we can't understand. Maybe it's
  // from the future?  Either we mis-encoded the object, or the
  // server sent us entries it shouldn't have.
  NOTREACHED() << "Unknown datatype in sync proto.";
  return UNSPECIFIED;
}

ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics) {
  if (specifics.HasExtension(sync_pb::bookmark))
    return BOOKMARKS;

  if (specifics.HasExtension(sync_pb::password))
    return PASSWORDS;

  if (specifics.HasExtension(sync_pb::preference))
    return PREFERENCES;

  if (specifics.HasExtension(sync_pb::autofill))
    return AUTOFILL;

  if (specifics.HasExtension(sync_pb::autofill_profile))
    return AUTOFILL_PROFILE;

  if (specifics.HasExtension(sync_pb::theme))
    return THEMES;

  if (specifics.HasExtension(sync_pb::typed_url))
    return TYPED_URLS;

  if (specifics.HasExtension(sync_pb::extension))
    return EXTENSIONS;

  if (specifics.HasExtension(sync_pb::nigori))
    return NIGORI;

  if (specifics.HasExtension(sync_pb::app))
    return APPS;

  if (specifics.HasExtension(sync_pb::session))
    return SESSIONS;

  return UNSPECIFIED;
}

std::string ModelTypeToString(ModelType model_type) {
  switch (model_type) {
    case BOOKMARKS:
      return "Bookmarks";
    case PREFERENCES:
      return "Preferences";
    case PASSWORDS:
      return "Passwords";
    case AUTOFILL:
      return "Autofill";
    case THEMES:
      return "Themes";
    case TYPED_URLS:
      return "Typed URLs";
    case EXTENSIONS:
      return "Extensions";
    case NIGORI:
      return "Encryption keys";
    case SESSIONS:
      return "Sessions";
    case APPS:
      return "Apps";
    case AUTOFILL_PROFILE:
      return "Autofill Profiles";
    default:
      break;
  }
  NOTREACHED() << "No known extension for model type.";
  return "INVALID";
}

std::string ModelTypeSetToString(const ModelTypeSet& model_types) {
  std::string result;
  for (ModelTypeSet::const_iterator iter = model_types.begin();
       iter != model_types.end();) {
    result += ModelTypeToString(*iter);
    if (++iter != model_types.end())
      result += ", ";
  }
  return result;
}

ModelType ModelTypeFromString(const std::string& model_type_string) {
  if (model_type_string == "Bookmarks")
    return BOOKMARKS;
  else if (model_type_string == "Preferences")
    return PREFERENCES;
  else if (model_type_string == "Passwords")
    return PASSWORDS;
  else if (model_type_string == "Autofill")
    return AUTOFILL;
  else if (model_type_string == "Autofill Profiles")
    return AUTOFILL_PROFILE;
  else if (model_type_string == "Themes")
    return THEMES;
  else if (model_type_string == "Typed URLs")
    return TYPED_URLS;
  else if (model_type_string == "Extensions")
    return EXTENSIONS;
  else if (model_type_string == "Encryption keys")
    return NIGORI;
  else if (model_type_string == "Sessions")
    return SESSIONS;
  else if (model_type_string == "Apps")
    return APPS;
  else
    NOTREACHED() << "No known model type corresponding to "
                 << model_type_string << ".";
  return UNSPECIFIED;
}

bool ModelTypeBitSetFromString(
    const std::string& model_type_bitset_string,
    ModelTypeBitSet* model_types) {
  DCHECK(model_types);
  if (model_type_bitset_string.length() != MODEL_TYPE_COUNT)
    return false;
  if (model_type_bitset_string.find_first_not_of("01") != std::string::npos)
    return false;
  *model_types = ModelTypeBitSet(model_type_bitset_string);
  return true;
}

ModelTypeBitSet ModelTypeBitSetFromSet(const ModelTypeSet& set) {
  ModelTypeBitSet bitset;
  for (ModelTypeSet::const_iterator iter = set.begin(); iter != set.end();
       ++iter) {
    bitset.set(*iter);
  }
  return bitset;
}

ListValue* ModelTypeBitSetToValue(const ModelTypeBitSet& model_types) {
  ListValue* value = new ListValue();
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (model_types[i]) {
      value->Append(
          Value::CreateStringValue(ModelTypeToString(ModelTypeFromInt(i))));
    }
  }
  return value;
}

ListValue* ModelTypeSetToValue(const ModelTypeSet& model_types) {
  ListValue* value = new ListValue();
  for (ModelTypeSet::const_iterator i = model_types.begin();
       i != model_types.end(); ++i) {
    value->Append(Value::CreateStringValue(ModelTypeToString(*i)));
  }
  return value;
}

// TODO(zea): remove all hardcoded tags in model associators and have them use
// this instead.
std::string ModelTypeToRootTag(ModelType type) {
  switch (type) {
    case BOOKMARKS:
      return "google_chrome_bookmarks";
    case PREFERENCES:
      return "google_chrome_preferences";
    case PASSWORDS:
      return "google_chrome_passwords";
    case AUTOFILL:
      return "google_chrome_autofill";
    case THEMES:
      return "google_chrome_themes";
    case TYPED_URLS:
      return "google_chrome_typed_urls";
    case EXTENSIONS:
      return "google_chrome_extensions";
    case NIGORI:
      return "google_chrome_nigori";
    case SESSIONS:
      return "google_chrome_sessions";
    case APPS:
      return "google_chrome_apps";
    case AUTOFILL_PROFILE:
      return "google_chrome_autofill_profiles";
    default:
      break;
  }
  NOTREACHED() << "No known extension for model type.";
  return "INVALID";
}

// For now, this just implements UMA_HISTOGRAM_LONG_TIMES. This can be adjusted
// if we feel the min, max, or bucket count amount are not appropriate.
#define SYNC_FREQ_HISTOGRAM(name, time) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, time, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromHours(1), 50)

void PostTimeToTypeHistogram(ModelType model_type, base::TimeDelta time) {
  switch (model_type) {
    case BOOKMARKS: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqBookmarks", time);
        return;
    }
    case PREFERENCES: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqPreferences", time);
        return;
    }
    case PASSWORDS: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqPasswords", time);
        return;
    }
    case AUTOFILL: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqAutofill", time);
        return;
    }
    case AUTOFILL_PROFILE: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqAutofillProfiles", time);
        return;
    }
    case THEMES: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqThemes", time);
        return;
    }
    case TYPED_URLS: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqTypedUrls", time);
        return;
    }
    case EXTENSIONS: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqExtensions", time);
        return;
    }
    case NIGORI: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqNigori", time);
        return;
    }
    case SESSIONS: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqSessions", time);
        return;
    }
    case APPS: {
        SYNC_FREQ_HISTOGRAM("Sync.FreqApps", time);
        return;
    }
    default:
      LOG(ERROR) << "No known extension for model type.";
  }
}

#undef SYNC_FREQ_HISTOGRAM

// TODO(akalin): Figure out a better way to do these mappings.

namespace {
const char kBookmarkNotificationType[] = "BOOKMARK";
const char kPreferenceNotificationType[] = "PREFERENCE";
const char kPasswordNotificationType[] = "PASSWORD";
const char kAutofillNotificationType[] = "AUTOFILL";
const char kThemeNotificationType[] = "THEME";
const char kTypedUrlNotificationType[] = "TYPED_URL";
const char kExtensionNotificationType[] = "EXTENSION";
const char kNigoriNotificationType[] = "NIGORI";
const char kAppNotificationType[] = "APP";
const char kSessionNotificationType[] = "SESSION";
const char kAutofillProfileNotificationType[] = "AUTOFILL_PROFILE";
}  // namespace

bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type) {
  switch (model_type) {
    case BOOKMARKS:
      *notification_type = kBookmarkNotificationType;
      return true;
    case PREFERENCES:
      *notification_type = kPreferenceNotificationType;
      return true;
    case PASSWORDS:
      *notification_type = kPasswordNotificationType;
      return true;
    case AUTOFILL:
      *notification_type = kAutofillNotificationType;
      return true;
    case THEMES:
      *notification_type = kThemeNotificationType;
      return true;
    case TYPED_URLS:
      *notification_type = kTypedUrlNotificationType;
      return true;
    case EXTENSIONS:
      *notification_type = kExtensionNotificationType;
      return true;
    case NIGORI:
      *notification_type = kNigoriNotificationType;
      return true;
    case APPS:
      *notification_type = kAppNotificationType;
      return true;
    case SESSIONS:
      *notification_type = kSessionNotificationType;
      return true;
    case AUTOFILL_PROFILE:
      *notification_type = kAutofillProfileNotificationType;
      return true;
    default:
      break;
  }
  notification_type->clear();
  return false;
}

bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type) {
  if (notification_type == kBookmarkNotificationType) {
    *model_type = BOOKMARKS;
    return true;
  } else if (notification_type == kPreferenceNotificationType) {
    *model_type = PREFERENCES;
    return true;
  } else if (notification_type == kPasswordNotificationType) {
    *model_type = PASSWORDS;
    return true;
  } else if (notification_type == kAutofillNotificationType) {
    *model_type = AUTOFILL;
    return true;
  } else if (notification_type == kThemeNotificationType) {
    *model_type = THEMES;
    return true;
  } else if (notification_type == kTypedUrlNotificationType) {
    *model_type = TYPED_URLS;
    return true;
  } else if (notification_type == kExtensionNotificationType) {
    *model_type = EXTENSIONS;
    return true;
  } else if (notification_type == kNigoriNotificationType) {
    *model_type = NIGORI;
    return true;
  } else if (notification_type == kAppNotificationType) {
    *model_type = APPS;
    return true;
  } else if (notification_type == kSessionNotificationType) {
    *model_type = SESSIONS;
    return true;
  } else if (notification_type == kAutofillProfileNotificationType) {
    *model_type = AUTOFILL_PROFILE;
    return true;
  }
  *model_type = UNSPECIFIED;
  return false;
}

}  // namespace syncable
