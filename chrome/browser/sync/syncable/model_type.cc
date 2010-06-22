// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type.h"

#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
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
    default:
      NOTREACHED() << "No known extension for model type.";
  }
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

  if (specifics.HasExtension(sync_pb::theme))
    return THEMES;

  if (specifics.HasExtension(sync_pb::typed_url))
    return TYPED_URLS;

  if (specifics.HasExtension(sync_pb::extension))
    return EXTENSIONS;

  if (specifics.HasExtension(sync_pb::nigori))
    return NIGORI;

  return UNSPECIFIED;
}

std::string ModelTypeToString(ModelType model_type) {
  switch (model_type) {
    case BOOKMARKS:
      return "Bookmarks";
    case PREFERENCES:
      return "Preferences";
    case AUTOFILL:
      return "Autofill";
    case THEMES:
      return "Themes";
    case TYPED_URLS:
      return "Typed URLs";
    case EXTENSIONS:
      return "Extensions";
    case PASSWORDS:
      return "Passwords";
    case NIGORI:
      return "Encryption keys";
    default:
      NOTREACHED() << "No known extension for model type.";
      return "INVALID";
  }
}

}  // namespace syncable
