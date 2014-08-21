// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"

#include <algorithm>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler_delegate.h"
#include "components/crx_file/id_util.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "extensions/common/extension.h"
#include "google/cacheinvalidation/types.pb.h"

namespace extensions {

namespace {

const int kNumberOfSubchannels = 4;

// Chrome push messaging object IDs currently have the following format:
// <format type>/<GAIA ID>/<extension ID>/<subchannel>
// <format type> must be 'U', and <GAIA ID> is handled server-side so the client
// never sees it.
syncer::ObjectIdSet ExtensionIdToObjectIds(const std::string& extension_id) {
  syncer::ObjectIdSet object_ids;
  for (int i = 0; i < kNumberOfSubchannels; ++i) {
    std::string name("U/");
    name += extension_id;
    name += "/";
    name += base::IntToString(i);
    object_ids.insert(invalidation::ObjectId(
        ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
        name));
  }
  return object_ids;
}

// Returns true iff the conversion was successful.
bool ObjectIdToExtensionAndSubchannel(const invalidation::ObjectId& object_id,
                                      std::string* extension_id,
                                      int* subchannel) {
  if (object_id.source() !=
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING) {
    DLOG(WARNING) << "Invalid source: " << object_id.source();
    return false;
  }

  const std::string& name = object_id.name();
  std::vector<std::string> components;
  base::SplitStringDontTrim(name, '/', &components);
  if (components.size() < 3) {
    DLOG(WARNING) << "Invalid format type from object name " << name;
    return false;
  }
  if (components[0] != "U") {
    DLOG(WARNING) << "Invalid format type from object name " << name;
    return false;
  }
  if (!crx_file::id_util::IdIsValid(components[1])) {
    DLOG(WARNING) << "Invalid extension ID from object name " << name;
    return false;
  }
  *extension_id = components[1];
  if (!base::StringToInt(components[2], subchannel)) {
    DLOG(WARNING) << "Subchannel not a number from object name " << name;
    return false;
  }
  if (*subchannel < 0 || *subchannel >= kNumberOfSubchannels) {
    DLOG(WARNING) << "Subchannel out of range from object name " << name;
    return false;
  }
  return true;
}

}  // namespace

PushMessagingInvalidationHandler::PushMessagingInvalidationHandler(
    invalidation::InvalidationService* service,
    PushMessagingInvalidationHandlerDelegate* delegate)
    : service_(service),
      delegate_(delegate) {
  DCHECK(service_);
  service_->RegisterInvalidationHandler(this);
}

PushMessagingInvalidationHandler::~PushMessagingInvalidationHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
  service_->UnregisterInvalidationHandler(this);
}

void PushMessagingInvalidationHandler::SuppressInitialInvalidationsForExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const syncer::ObjectIdSet& suppressed_ids =
      ExtensionIdToObjectIds(extension_id);
  suppressed_ids_.insert(suppressed_ids.begin(), suppressed_ids.end());
}

void PushMessagingInvalidationHandler::RegisterExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  registered_extensions_.insert(extension_id);
  UpdateRegistrations();
}

void PushMessagingInvalidationHandler::UnregisterExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  registered_extensions_.erase(extension_id);
  UpdateRegistrations();
}

void PushMessagingInvalidationHandler::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Nothing to do.
}

void PushMessagingInvalidationHandler::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  invalidation_map.AcknowledgeAll();

  syncer::ObjectIdSet ids = invalidation_map.GetObjectIds();
  for (syncer::ObjectIdSet::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    const syncer::SingleObjectInvalidationSet& list =
        invalidation_map.ForObject(*it);
    const syncer::Invalidation& invalidation = list.back();

    std::string payload;
    if (invalidation.is_unknown_version()) {
      payload = std::string();
    } else {
      payload = list.back().payload();
    }

    syncer::ObjectIdSet::iterator suppressed_id = suppressed_ids_.find(*it);
    if (suppressed_id != suppressed_ids_.end()) {
      suppressed_ids_.erase(suppressed_id);
      continue;
    }
    DVLOG(2) << "Incoming push message, id is: "
             << syncer::ObjectIdToString(*it)
             << " and payload is:" << payload;

    std::string extension_id;
    int subchannel;
    if (ObjectIdToExtensionAndSubchannel(*it, &extension_id, &subchannel)) {
      const syncer::SingleObjectInvalidationSet& invalidation_list =
          invalidation_map.ForObject(*it);

      // We always forward unknown version invalidation when we receive one.
      if (invalidation_list.StartsWithUnknownVersion()) {
        DVLOG(2) << "Sending push message to receiver, extension is "
            << extension_id << ", subchannel is " << subchannel
            << "and payload was lost";
        delegate_->OnMessage(extension_id, subchannel, std::string());
      }

      // If we receive a new max version for this object, forward its payload.
      const syncer::Invalidation& max_invalidation = invalidation_list.back();
      if (!max_invalidation.is_unknown_version() &&
          max_invalidation.version() > max_object_version_map_[*it]) {
        max_object_version_map_[*it] = max_invalidation.version();
        DVLOG(2) << "Sending push message to receiver, extension is "
            << extension_id << ", subchannel is " << subchannel
            << ", and payload is " << max_invalidation.payload();
        delegate_->OnMessage(extension_id,
                             subchannel,
                             max_invalidation.payload());
      }
    }
  }
}

std::string PushMessagingInvalidationHandler::GetOwnerName() const {
  return "PushMessagingApi";
}

void PushMessagingInvalidationHandler::UpdateRegistrations() {
  syncer::ObjectIdSet ids;
  for (std::set<std::string>::const_iterator it =
       registered_extensions_.begin(); it != registered_extensions_.end();
       ++it) {
    const syncer::ObjectIdSet& object_ids = ExtensionIdToObjectIds(*it);
    ids.insert(object_ids.begin(), object_ids.end());
  }
  service_->UpdateRegisteredInvalidationIds(this, ids);
}

}  // namespace extensions
