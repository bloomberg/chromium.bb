// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type.h"

#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"

namespace syncable {

void AddDefaultExtensionValue(syncable::ModelType datatype,
                              sync_pb::EntitySpecifics* specifics) {
  switch (datatype) {
    case BOOKMARKS:
      specifics->MutableExtension(sync_pb::bookmark);
      break;
    case PREFERENCES:
      specifics->MutableExtension(sync_pb::preference);
      break;
    case AUTOFILL:
      specifics->MutableExtension(sync_pb::autofill);
      break;
    case TYPED_URLS:
      specifics->MutableExtension(sync_pb::typed_url);
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
    return syncable::UNSPECIFIED;

  if (sync_entity.specifics().HasExtension(sync_pb::bookmark) ||
      sync_entity.has_bookmarkdata()) {
    return syncable::BOOKMARKS;
  }

  if (sync_entity.specifics().HasExtension(sync_pb::preference))
    return syncable::PREFERENCES;

  if (sync_entity.specifics().HasExtension(sync_pb::autofill))
    return syncable::AUTOFILL;

  if (sync_entity.specifics().HasExtension(sync_pb::typed_url))
    return syncable::TYPED_URLS;

  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!sync_entity.server_defined_unique_tag().empty() &&
      sync_entity.IsFolder()) {
    return syncable::TOP_LEVEL_FOLDER;
  }

  // This is an item of a datatype we can't understand. Maybe it's
  // from the future?  Either we mis-encoded the object, or the
  // server sent us entries it shouldn't have.
  NOTREACHED() << "Unknown datatype in sync proto.";
  return syncable::UNSPECIFIED;
}

}  // namespace syncable
