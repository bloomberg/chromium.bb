// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/base_node.h"

#include "base/base64.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/syncapi_internal.h"
#include "chrome/browser/sync/internal_api/base_transaction.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/util/time.h"

using syncable::SPECIFICS;
using sync_pb::AutofillProfileSpecifics;

namespace sync_api {

// Helper function to look up the int64 metahandle of an object given the ID
// string.
static int64 IdToMetahandle(syncable::BaseTransaction* trans,
                            const syncable::Id& id) {
  syncable::Entry entry(trans, syncable::GET_BY_ID, id);
  if (!entry.good())
    return kInvalidId;
  return entry.Get(syncable::META_HANDLE);
}

static bool EndsWithSpace(const std::string& string) {
  return !string.empty() && *string.rbegin() == ' ';
}

// In the reverse direction, if a server name matches the pattern of a
// server-illegal name followed by one or more spaces, remove the trailing
// space.
static void ServerNameToSyncAPIName(const std::string& server_name,
                                    std::string* out) {
  CHECK(out);
  int length_to_copy = server_name.length();
  if (IsNameServerIllegalAfterTrimming(server_name) &&
      EndsWithSpace(server_name)) {
    --length_to_copy;
  }
  *out = std::string(server_name.c_str(), length_to_copy);
}

BaseNode::BaseNode() : password_data_(new sync_pb::PasswordSpecificsData) {}

BaseNode::~BaseNode() {}

std::string BaseNode::GenerateSyncableHash(
    syncable::ModelType model_type, const std::string& client_tag) {
  // blank PB with just the extension in it has termination symbol,
  // handy for delimiter
  sync_pb::EntitySpecifics serialized_type;
  syncable::AddDefaultExtensionValue(model_type, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(client_tag);

  std::string encode_output;
  CHECK(base::Base64Encode(base::SHA1HashString(hash_input), &encode_output));
  return encode_output;
}

bool BaseNode::DecryptIfNecessary() {
  if (!GetEntry()->Get(syncable::UNIQUE_SERVER_TAG).empty())
      return true;  // Ignore unique folders.
  const sync_pb::EntitySpecifics& specifics =
      GetEntry()->Get(syncable::SPECIFICS);
  if (specifics.HasExtension(sync_pb::password)) {
    // Passwords have their own legacy encryption structure.
    scoped_ptr<sync_pb::PasswordSpecificsData> data(DecryptPasswordSpecifics(
        specifics, GetTransaction()->GetCryptographer()));
    if (!data.get()) {
      LOG(ERROR) << "Failed to decrypt password specifics.";
      return false;
    }
    password_data_.swap(data);
    return true;
  }

  // We assume any node with the encrypted field set has encrypted data and if
  // not we have no work to do, with the exception of bookmarks. For bookmarks
  // we must make sure the bookmarks data has the title field supplied. If not,
  // we fill the unencrypted_data_ with a copy of the bookmark specifics that
  // follows the new bookmarks format.
  if (!specifics.has_encrypted()) {
    if (GetModelType() == syncable::BOOKMARKS &&
        !specifics.GetExtension(sync_pb::bookmark).has_title() &&
        !GetTitle().empty()) {  // Last check ensures this isn't a new node.
      // We need to fill in the title.
      std::string title = GetTitle();
      std::string server_legal_title;
      SyncAPINameToServerName(title, &server_legal_title);
      DVLOG(1) << "Reading from legacy bookmark, manually returning title "
               << title;
      unencrypted_data_.CopyFrom(specifics);
      unencrypted_data_.MutableExtension(sync_pb::bookmark)->set_title(
          server_legal_title);
    }
    return true;
  }

  const sync_pb::EncryptedData& encrypted = specifics.encrypted();
  std::string plaintext_data = GetTransaction()->GetCryptographer()->
      DecryptToString(encrypted);
  if (plaintext_data.length() == 0 ||
      !unencrypted_data_.ParseFromString(plaintext_data)) {
    LOG(ERROR) << "Failed to decrypt encrypted node of type " <<
      syncable::ModelTypeToString(GetModelType()) << ".";
    return false;
  }
  DVLOG(2) << "Decrypted specifics of type "
           << syncable::ModelTypeToString(GetModelType())
           << " with content: " << plaintext_data;
  return true;
}

const sync_pb::EntitySpecifics& BaseNode::GetUnencryptedSpecifics(
    const syncable::Entry* entry) const {
  const sync_pb::EntitySpecifics& specifics = entry->Get(SPECIFICS);
  if (specifics.has_encrypted()) {
    DCHECK_NE(syncable::GetModelTypeFromSpecifics(unencrypted_data_),
              syncable::UNSPECIFIED);
    return unencrypted_data_;
  } else {
    // Due to the change in bookmarks format, we need to check to see if this is
    // a legacy bookmarks (and has no title field in the proto). If it is, we
    // return the unencrypted_data_, which was filled in with the title by
    // DecryptIfNecessary().
    if (GetModelType() == syncable::BOOKMARKS) {
      const sync_pb::BookmarkSpecifics& bookmark_specifics =
          specifics.GetExtension(sync_pb::bookmark);
      if (bookmark_specifics.has_title() ||
          GetTitle().empty() ||  // For the empty node case
          !GetEntry()->Get(syncable::UNIQUE_SERVER_TAG).empty()) {
        // It's possible we previously had to convert and set
        // |unencrypted_data_| but then wrote our own data, so we allow
        // |unencrypted_data_| to be non-empty.
        return specifics;
      } else {
        DCHECK_EQ(syncable::GetModelTypeFromSpecifics(unencrypted_data_),
                  syncable::BOOKMARKS);
        return unencrypted_data_;
      }
    } else {
      DCHECK_EQ(syncable::GetModelTypeFromSpecifics(unencrypted_data_),
                syncable::UNSPECIFIED);
      return specifics;
    }
  }
}

int64 BaseNode::GetParentId() const {
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(),
                        GetEntry()->Get(syncable::PARENT_ID));
}

int64 BaseNode::GetId() const {
  return GetEntry()->Get(syncable::META_HANDLE);
}

const base::Time& BaseNode::GetModificationTime() const {
  return GetEntry()->Get(syncable::MTIME);
}

bool BaseNode::GetIsFolder() const {
  return GetEntry()->Get(syncable::IS_DIR);
}

std::string BaseNode::GetTitle() const {
  std::string result;
  // TODO(zea): refactor bookmarks to not need this functionality.
  if (syncable::BOOKMARKS == GetModelType() &&
      GetEntry()->Get(syncable::SPECIFICS).has_encrypted()) {
    // Special case for legacy bookmarks dealing with encryption.
    ServerNameToSyncAPIName(GetBookmarkSpecifics().title(), &result);
  } else {
    ServerNameToSyncAPIName(GetEntry()->Get(syncable::NON_UNIQUE_NAME),
                            &result);
  }
  return result;
}

GURL BaseNode::GetURL() const {
  return GURL(GetBookmarkSpecifics().url());
}

bool BaseNode::HasChildren() const {
  syncable::Directory* dir = GetTransaction()->GetLookup();
  syncable::BaseTransaction* trans = GetTransaction()->GetWrappedTrans();
  return dir->HasChildren(trans, GetEntry()->Get(syncable::ID));
}

int64 BaseNode::GetPredecessorId() const {
  syncable::Id id_string = GetEntry()->Get(syncable::PREV_ID);
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64 BaseNode::GetSuccessorId() const {
  syncable::Id id_string = GetEntry()->Get(syncable::NEXT_ID);
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64 BaseNode::GetFirstChildId() const {
  syncable::Directory* dir = GetTransaction()->GetLookup();
  syncable::BaseTransaction* trans = GetTransaction()->GetWrappedTrans();
  syncable::Id id_string;
  // TODO(akalin): Propagate up the error further (see
  // http://crbug.com/100907).
  CHECK(dir->GetFirstChildId(trans,
                             GetEntry()->Get(syncable::ID), &id_string));
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

DictionaryValue* BaseNode::GetSummaryAsValue() const {
  DictionaryValue* node_info = new DictionaryValue();
  node_info->SetString("id", base::Int64ToString(GetId()));
  node_info->SetBoolean("isFolder", GetIsFolder());
  node_info->SetString("title", GetTitle());
  node_info->Set("type", ModelTypeToValue(GetModelType()));
  return node_info;
}

DictionaryValue* BaseNode::GetDetailsAsValue() const {
  DictionaryValue* node_info = GetSummaryAsValue();
  node_info->SetString(
      "modificationTime",
      browser_sync::GetTimeDebugString(GetModificationTime()));
  node_info->SetString("parentId", base::Int64ToString(GetParentId()));
  // Specifics are already in the Entry value, so no need to duplicate
  // it here.
  node_info->SetString("externalId",
                       base::Int64ToString(GetExternalId()));
  node_info->SetString("predecessorId",
                       base::Int64ToString(GetPredecessorId()));
  node_info->SetString("successorId",
                       base::Int64ToString(GetSuccessorId()));
  node_info->SetString("firstChildId",
                       base::Int64ToString(GetFirstChildId()));
  node_info->Set("entry", GetEntry()->ToValue());
  return node_info;
}

void BaseNode::GetFaviconBytes(std::vector<unsigned char>* output) const {
  if (!output)
    return;
  const std::string& favicon = GetBookmarkSpecifics().favicon();
  output->assign(reinterpret_cast<const unsigned char*>(favicon.data()),
      reinterpret_cast<const unsigned char*>(favicon.data() +
                                             favicon.length()));
}

int64 BaseNode::GetExternalId() const {
  return GetEntry()->Get(syncable::LOCAL_EXTERNAL_ID);
}

const sync_pb::AppSpecifics& BaseNode::GetAppSpecifics() const {
  DCHECK_EQ(syncable::APPS, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::app);
}

const sync_pb::AutofillSpecifics& BaseNode::GetAutofillSpecifics() const {
  DCHECK_EQ(syncable::AUTOFILL, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::autofill);
}

const AutofillProfileSpecifics& BaseNode::GetAutofillProfileSpecifics() const {
  DCHECK_EQ(GetModelType(), syncable::AUTOFILL_PROFILE);
  return GetEntitySpecifics().GetExtension(sync_pb::autofill_profile);
}

const sync_pb::BookmarkSpecifics& BaseNode::GetBookmarkSpecifics() const {
  DCHECK_EQ(syncable::BOOKMARKS, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::bookmark);
}

const sync_pb::NigoriSpecifics& BaseNode::GetNigoriSpecifics() const {
  DCHECK_EQ(syncable::NIGORI, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::nigori);
}

const sync_pb::PasswordSpecificsData& BaseNode::GetPasswordSpecifics() const {
  DCHECK_EQ(syncable::PASSWORDS, GetModelType());
  return *password_data_;
}

const sync_pb::ThemeSpecifics& BaseNode::GetThemeSpecifics() const {
  DCHECK_EQ(syncable::THEMES, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::theme);
}

const sync_pb::TypedUrlSpecifics& BaseNode::GetTypedUrlSpecifics() const {
  DCHECK_EQ(syncable::TYPED_URLS, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::typed_url);
}

const sync_pb::ExtensionSpecifics& BaseNode::GetExtensionSpecifics() const {
  DCHECK_EQ(syncable::EXTENSIONS, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::extension);
}

const sync_pb::SessionSpecifics& BaseNode::GetSessionSpecifics() const {
  DCHECK_EQ(syncable::SESSIONS, GetModelType());
  return GetEntitySpecifics().GetExtension(sync_pb::session);
}

const sync_pb::EntitySpecifics& BaseNode::GetEntitySpecifics() const {
  return GetUnencryptedSpecifics(GetEntry());
}

syncable::ModelType BaseNode::GetModelType() const {
  return GetEntry()->GetModelType();
}

void BaseNode::SetUnencryptedSpecifics(
    const sync_pb::EntitySpecifics& specifics) {
  syncable::ModelType type = syncable::GetModelTypeFromSpecifics(specifics);
  DCHECK_NE(syncable::UNSPECIFIED, type);
  if (GetModelType() != syncable::UNSPECIFIED) {
    DCHECK_EQ(GetModelType(), type);
  }
  unencrypted_data_.CopyFrom(specifics);
}

}  // namespace sync_api
