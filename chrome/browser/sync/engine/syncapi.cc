// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncapi.h"

#include "build/build_config.h"

#include <iomanip>
#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/base64.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/scoped_ptr.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/sync/sync_constants.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/change_reorder_buffer.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/net/syncapi_server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/notifier/server_notifier_thread.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/deprecated/event_sys.h"
#include "chrome/common/net/gaia/gaia_authenticator.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/talk_mediator.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"
#include "net/base/network_change_notifier.h"

using browser_sync::AllStatus;
using browser_sync::AllStatusEvent;
using browser_sync::AuthWatcher;
using browser_sync::AuthWatcherEvent;
using browser_sync::Cryptographer;
using browser_sync::KeyParams;
using browser_sync::ModelSafeRoutingInfo;
using browser_sync::ModelSafeWorker;
using browser_sync::ModelSafeWorkerRegistrar;
using browser_sync::Syncer;
using browser_sync::SyncerEvent;
using browser_sync::SyncerThread;
using browser_sync::UserSettings;
using browser_sync::kNigoriTag;
using browser_sync::sessions::SyncSessionContext;
using notifier::TalkMediator;
using notifier::TalkMediatorImpl;
using std::list;
using std::hex;
using std::string;
using std::vector;
using syncable::Directory;
using syncable::DirectoryManager;
using syncable::Entry;
using syncable::SPECIFICS;

typedef GoogleServiceAuthError AuthError;

static const int kThreadExitTimeoutMsec = 60000;
static const int kSSLPort = 443;

// We manage the lifetime of sync_api::SyncManager::SyncInternal ourselves.
DISABLE_RUNNABLE_METHOD_REFCOUNT(sync_api::SyncManager::SyncInternal);

namespace sync_api {

static const FilePath::CharType kBookmarkSyncUserSettingsDatabase[] =
    FILE_PATH_LITERAL("BookmarkSyncSettings.sqlite3");
static const char kDefaultNameForNewNodes[] = " ";

// The list of names which are reserved for use by the server.
static const char* kForbiddenServerNames[] = { "", ".", ".." };

//////////////////////////////////////////////////////////////////////////
// Static helper functions.

// Helper function to look up the int64 metahandle of an object given the ID
// string.
static int64 IdToMetahandle(syncable::BaseTransaction* trans,
                            const syncable::Id& id) {
  syncable::Entry entry(trans, syncable::GET_BY_ID, id);
  if (!entry.good())
    return kInvalidId;
  return entry.Get(syncable::META_HANDLE);
}

// Checks whether |name| is a server-illegal name followed by zero or more space
// characters.  The three server-illegal names are the empty string, dot, and
// dot-dot.  Very long names (>255 bytes in UTF-8 Normalization Form C) are
// also illegal, but are not considered here.
static bool IsNameServerIllegalAfterTrimming(const std::string& name) {
  size_t untrimmed_count = name.find_last_not_of(' ') + 1;
  for (size_t i = 0; i < arraysize(kForbiddenServerNames); ++i) {
    if (name.compare(0, untrimmed_count, kForbiddenServerNames[i]) == 0)
      return true;
  }
  return false;
}

static bool EndsWithSpace(const std::string& string) {
  return !string.empty() && *string.rbegin() == ' ';
}

// When taking a name from the syncapi, append a space if it matches the
// pattern of a server-illegal name followed by zero or more spaces.
static void SyncAPINameToServerName(const std::wstring& sync_api_name,
                                    std::string* out) {
  *out = WideToUTF8(sync_api_name);
  if (IsNameServerIllegalAfterTrimming(*out))
    out->append(" ");
}

// In the reverse direction, if a server name matches the pattern of a
// server-illegal name followed by one or more spaces, remove the trailing
// space.
static void ServerNameToSyncAPIName(const std::string& server_name,
                                    std::wstring* out) {
  int length_to_copy = server_name.length();
  if (IsNameServerIllegalAfterTrimming(server_name) &&
      EndsWithSpace(server_name))
    --length_to_copy;
  if (!UTF8ToWide(server_name.c_str(), length_to_copy, out)) {
    NOTREACHED() << "Could not convert server name from UTF8 to wide";
  }
}

////////////////////////////////////
// BaseNode member definitions.

BaseNode::BaseNode() {}

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

bool BaseNode::DecryptIfNecessary(Entry* entry) {
  if (GetIsFolder()) return true;  // Ignore the top-level password folder.
  const sync_pb::EntitySpecifics& specifics =
      entry->Get(syncable::SPECIFICS);
  if (specifics.HasExtension(sync_pb::password)) {
    const sync_pb::EncryptedData& encrypted =
        specifics.GetExtension(sync_pb::password).encrypted();
    scoped_ptr<sync_pb::PasswordSpecificsData> data(
        new sync_pb::PasswordSpecificsData);
    if (!GetTransaction()->GetCryptographer()->Decrypt(encrypted,
                                                       data.get()))
      return false;
    password_data_.swap(data);
  }
  return true;
}

int64 BaseNode::GetParentId() const {
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(),
                        GetEntry()->Get(syncable::PARENT_ID));
}

int64 BaseNode::GetId() const {
  return GetEntry()->Get(syncable::META_HANDLE);
}

bool BaseNode::GetIsFolder() const {
  return GetEntry()->Get(syncable::IS_DIR);
}

std::wstring BaseNode::GetTitle() const {
  std::wstring result;
  ServerNameToSyncAPIName(GetEntry()->Get(syncable::NON_UNIQUE_NAME), &result);
  return result;
}

GURL BaseNode::GetURL() const {
  return GURL(GetBookmarkSpecifics().url());
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
  syncable::Id id_string =
      dir->GetFirstChildId(trans, GetEntry()->Get(syncable::ID));
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
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
  DCHECK(GetModelType() == syncable::APPS);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::app);
}

const sync_pb::AutofillSpecifics& BaseNode::GetAutofillSpecifics() const {
  DCHECK(GetModelType() == syncable::AUTOFILL);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::autofill);
}

const sync_pb::BookmarkSpecifics& BaseNode::GetBookmarkSpecifics() const {
  DCHECK(GetModelType() == syncable::BOOKMARKS);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::bookmark);
}

const sync_pb::NigoriSpecifics& BaseNode::GetNigoriSpecifics() const {
  DCHECK(GetModelType() == syncable::NIGORI);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::nigori);
}

const sync_pb::PasswordSpecificsData& BaseNode::GetPasswordSpecifics() const {
  DCHECK(GetModelType() == syncable::PASSWORDS);
  DCHECK(password_data_.get());
  return *password_data_;
}

const sync_pb::PreferenceSpecifics& BaseNode::GetPreferenceSpecifics() const {
  DCHECK(GetModelType() == syncable::PREFERENCES);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::preference);
}

const sync_pb::ThemeSpecifics& BaseNode::GetThemeSpecifics() const {
  DCHECK(GetModelType() == syncable::THEMES);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::theme);
}

const sync_pb::TypedUrlSpecifics& BaseNode::GetTypedUrlSpecifics() const {
  DCHECK(GetModelType() == syncable::TYPED_URLS);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::typed_url);
}

const sync_pb::ExtensionSpecifics& BaseNode::GetExtensionSpecifics() const {
  DCHECK(GetModelType() == syncable::EXTENSIONS);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::extension);
}

const sync_pb::SessionSpecifics& BaseNode::GetSessionSpecifics() const {
  DCHECK(GetModelType() == syncable::SESSIONS);
  return GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::session);
}

syncable::ModelType BaseNode::GetModelType() const {
  return GetEntry()->GetModelType();
}

////////////////////////////////////
// WriteNode member definitions
void WriteNode::SetIsFolder(bool folder) {
  if (entry_->Get(syncable::IS_DIR) == folder)
    return;  // Skip redundant changes.

  entry_->Put(syncable::IS_DIR, folder);
  MarkForSyncing();
}

void WriteNode::SetTitle(const std::wstring& title) {
  std::string server_legal_name;
  SyncAPINameToServerName(title, &server_legal_name);

  string old_name = entry_->Get(syncable::NON_UNIQUE_NAME);

  if (server_legal_name == old_name)
    return;  // Skip redundant changes.

  entry_->Put(syncable::NON_UNIQUE_NAME, server_legal_name);
  MarkForSyncing();
}

void WriteNode::SetURL(const GURL& url) {
  sync_pb::BookmarkSpecifics new_value = GetBookmarkSpecifics();
  new_value.set_url(url.spec());
  SetBookmarkSpecifics(new_value);
}

void WriteNode::SetAppSpecifics(
    const sync_pb::AppSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::APPS);
  PutAppSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::SetAutofillSpecifics(
    const sync_pb::AutofillSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::AUTOFILL);
  PutAutofillSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::PutAutofillSpecificsAndMarkForSyncing(
    const sync_pb::AutofillSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::autofill)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::SetBookmarkSpecifics(
    const sync_pb::BookmarkSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::BOOKMARKS);
  PutBookmarkSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::PutBookmarkSpecificsAndMarkForSyncing(
    const sync_pb::BookmarkSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::bookmark)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::SetNigoriSpecifics(
    const sync_pb::NigoriSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::NIGORI);
  PutNigoriSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::PutNigoriSpecificsAndMarkForSyncing(
    const sync_pb::NigoriSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::nigori)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::SetPasswordSpecifics(
    const sync_pb::PasswordSpecificsData& data) {
  DCHECK(GetModelType() == syncable::PASSWORDS);
  std::string serialized_data;
  data.SerializeToString(&serialized_data);
  sync_pb::PasswordSpecifics new_value;
  if (!GetTransaction()->GetCryptographer()->Encrypt(
      data,
      new_value.mutable_encrypted()))
    NOTREACHED();

  PutPasswordSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::SetPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::PREFERENCES);
  PutPreferenceSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::SetThemeSpecifics(
    const sync_pb::ThemeSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::THEMES);
  PutThemeSpecificsAndMarkForSyncing(new_value);
}


void WriteNode::SetSessionSpecifics(
    const sync_pb::SessionSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::SESSIONS);
  PutSessionSpecificsAndMarkForSyncing(new_value);
}


void WriteNode::PutPasswordSpecificsAndMarkForSyncing(
    const sync_pb::PasswordSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::password)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::PutPreferenceSpecificsAndMarkForSyncing(
    const sync_pb::PreferenceSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::preference)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::SetTypedUrlSpecifics(
    const sync_pb::TypedUrlSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::TYPED_URLS);
  PutTypedUrlSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::SetExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& new_value) {
  DCHECK(GetModelType() == syncable::EXTENSIONS);
  PutExtensionSpecificsAndMarkForSyncing(new_value);
}

void WriteNode::PutAppSpecificsAndMarkForSyncing(
    const sync_pb::AppSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::app)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::PutThemeSpecificsAndMarkForSyncing(
    const sync_pb::ThemeSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::theme)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::PutTypedUrlSpecificsAndMarkForSyncing(
    const sync_pb::TypedUrlSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::typed_url)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}

void WriteNode::PutExtensionSpecificsAndMarkForSyncing(
    const sync_pb::ExtensionSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::extension)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}


void WriteNode::PutSessionSpecificsAndMarkForSyncing(
    const sync_pb::SessionSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::session)->CopyFrom(new_value);
  PutSpecificsAndMarkForSyncing(entity_specifics);
}


void WriteNode::PutSpecificsAndMarkForSyncing(
    const sync_pb::EntitySpecifics& specifics) {
  // Skip redundant changes.
  if (specifics.SerializeAsString() ==
      entry_->Get(SPECIFICS).SerializeAsString()) {
    return;
  }
  entry_->Put(SPECIFICS, specifics);
  MarkForSyncing();
}

void WriteNode::SetExternalId(int64 id) {
  if (GetExternalId() != id)
    entry_->Put(syncable::LOCAL_EXTERNAL_ID, id);
}

WriteNode::WriteNode(WriteTransaction* transaction)
    : entry_(NULL), transaction_(transaction) {
  DCHECK(transaction);
}

WriteNode::~WriteNode() {
  delete entry_;
}

// Find an existing node matching the ID |id|, and bind this WriteNode to it.
// Return true on success.
bool WriteNode::InitByIdLookup(int64 id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_BY_HANDLE, id);
  return (entry_->good() && !entry_->Get(syncable::IS_DEL) &&
          DecryptIfNecessary(entry_));
}

// Find a node by client tag, and bind this WriteNode to it.
// Return true if the write node was found, and was not deleted.
// Undeleting a deleted node is possible by ClientTag.
bool WriteNode::InitByClientTagLookup(syncable::ModelType model_type,
                                      const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return false;

  const std::string hash = GenerateSyncableHash(model_type, tag);

  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_BY_CLIENT_TAG, hash);
  return (entry_->good() && !entry_->Get(syncable::IS_DEL) &&
          DecryptIfNecessary(entry_));
}

bool WriteNode::InitByTagLookup(const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return false;
  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_BY_SERVER_TAG, tag);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  syncable::ModelType model_type = GetModelType();
  DCHECK(model_type == syncable::NIGORI);
  return true;
}

void WriteNode::PutModelType(syncable::ModelType model_type) {
  // Set an empty specifics of the appropriate datatype.  The presence
  // of the specific extension will identify the model type.
  DCHECK(GetModelType() == model_type ||
         GetModelType() == syncable::UNSPECIFIED);  // Immutable once set.

  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultExtensionValue(model_type, &specifics);
  PutSpecificsAndMarkForSyncing(specifics);
  DCHECK(GetModelType() == model_type);
}

// Create a new node with default properties, and bind this WriteNode to it.
// Return true on success.
bool WriteNode::InitByCreation(syncable::ModelType model_type,
                               const BaseNode& parent,
                               const BaseNode* predecessor) {
  DCHECK(!entry_) << "Init called twice";
  // |predecessor| must be a child of |parent| or NULL.
  if (predecessor && predecessor->GetParentId() != parent.GetId()) {
    DCHECK(false);
    return false;
  }

  syncable::Id parent_id = parent.GetEntry()->Get(syncable::ID);

  // Start out with a dummy name.  We expect
  // the caller to set a meaningful name after creation.
  string dummy(kDefaultNameForNewNodes);

  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::CREATE, parent_id, dummy);

  if (!entry_->good())
    return false;

  // Entries are untitled folders by default.
  entry_->Put(syncable::IS_DIR, true);

  PutModelType(model_type);

  // Now set the predecessor, which sets IS_UNSYNCED as necessary.
  PutPredecessor(predecessor);

  return true;
}

// Create a new node with default properties and a client defined unique tag,
// and bind this WriteNode to it.
// Return true on success. If the tag exists in the database, then
// we will attempt to undelete the node.
// TODO(chron): Code datatype into hash tag.
// TODO(chron): Is model type ever lost?
bool WriteNode::InitUniqueByCreation(syncable::ModelType model_type,
                                     const BaseNode& parent,
                                     const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";

  const std::string hash = GenerateSyncableHash(model_type, tag);

  syncable::Id parent_id = parent.GetEntry()->Get(syncable::ID);

  // Start out with a dummy name.  We expect
  // the caller to set a meaningful name after creation.
  string dummy(kDefaultNameForNewNodes);

  // Check if we have this locally and need to undelete it.
  scoped_ptr<syncable::MutableEntry> existing_entry(
      new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                 syncable::GET_BY_CLIENT_TAG, hash));

  if (existing_entry->good()) {
    if (existing_entry->Get(syncable::IS_DEL)) {
      // Rules for undelete:
      // BASE_VERSION: Must keep the same.
      // ID: Essential to keep the same.
      // META_HANDLE: Must be the same, so we can't "split" the entry.
      // IS_DEL: Must be set to false, will cause reindexing.
      //         This one is weird because IS_DEL is true for "update only"
      //         items. It should be OK to undelete an update only.
      // MTIME/CTIME: Seems reasonable to just leave them alone.
      // IS_UNSYNCED: Must set this to true or face database insurrection.
      //              We do this below this block.
      // IS_UNAPPLIED_UPDATE: Either keep it the same or also set BASE_VERSION
      //                      to SERVER_VERSION. We keep it the same here.
      // IS_DIR: We'll leave it the same.
      // SPECIFICS: Reset it.

      existing_entry->Put(syncable::IS_DEL, false);

      // Client tags are immutable and must be paired with the ID.
      // If a server update comes down with an ID and client tag combo,
      // and it already exists, always overwrite it and store only one copy.
      // We have to undelete entries because we can't disassociate IDs from
      // tags and updates.

      existing_entry->Put(syncable::NON_UNIQUE_NAME, dummy);
      existing_entry->Put(syncable::PARENT_ID, parent_id);
      entry_ = existing_entry.release();
    } else {
      return false;
    }
  } else {
    entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                        syncable::CREATE, parent_id, dummy);
    if (!entry_->good()) {
      return false;
    }

    // Only set IS_DIR for new entries. Don't bitflip undeleted ones.
    entry_->Put(syncable::UNIQUE_CLIENT_TAG, hash);
  }

  // We don't support directory and tag combinations.
  entry_->Put(syncable::IS_DIR, false);

  // Will clear specifics data.
  PutModelType(model_type);

  // Now set the predecessor, which sets IS_UNSYNCED as necessary.
  PutPredecessor(NULL);

  return true;
}

bool WriteNode::SetPosition(const BaseNode& new_parent,
                            const BaseNode* predecessor) {
  // |predecessor| must be a child of |new_parent| or NULL.
  if (predecessor && predecessor->GetParentId() != new_parent.GetId()) {
    DCHECK(false);
    return false;
  }

  syncable::Id new_parent_id = new_parent.GetEntry()->Get(syncable::ID);

  // Filter out redundant changes if both the parent and the predecessor match.
  if (new_parent_id == entry_->Get(syncable::PARENT_ID)) {
    const syncable::Id& old = entry_->Get(syncable::PREV_ID);
    if ((!predecessor && old.IsRoot()) ||
        (predecessor && (old == predecessor->GetEntry()->Get(syncable::ID)))) {
      return true;
    }
  }

  // Atomically change the parent. This will fail if it would
  // introduce a cycle in the hierarchy.
  if (!entry_->Put(syncable::PARENT_ID, new_parent_id))
    return false;

  // Now set the predecessor, which sets IS_UNSYNCED as necessary.
  PutPredecessor(predecessor);

  return true;
}

const syncable::Entry* WriteNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* WriteNode::GetTransaction() const {
  return transaction_;
}

void WriteNode::Remove() {
  entry_->Put(syncable::IS_DEL, true);
  MarkForSyncing();
}

void WriteNode::PutPredecessor(const BaseNode* predecessor) {
  syncable::Id predecessor_id = predecessor ?
      predecessor->GetEntry()->Get(syncable::ID) : syncable::Id();
  entry_->PutPredecessor(predecessor_id);
  // Mark this entry as unsynced, to wake up the syncer.
  MarkForSyncing();
}

void WriteNode::SetFaviconBytes(const vector<unsigned char>& bytes) {
  sync_pb::BookmarkSpecifics new_value = GetBookmarkSpecifics();
  new_value.set_favicon(bytes.empty() ? NULL : &bytes[0], bytes.size());
  SetBookmarkSpecifics(new_value);
}

void WriteNode::MarkForSyncing() {
  syncable::MarkForSyncing(entry_);
}

//////////////////////////////////////////////////////////////////////////
// ReadNode member definitions
ReadNode::ReadNode(const BaseTransaction* transaction)
    : entry_(NULL), transaction_(transaction) {
  DCHECK(transaction);
}

ReadNode::~ReadNode() {
  delete entry_;
}

void ReadNode::InitByRootLookup() {
  DCHECK(!entry_) << "Init called twice";
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_ID, trans->root_id());
  if (!entry_->good())
    DCHECK(false) << "Could not lookup root node for reading.";
}

bool ReadNode::InitByIdLookup(int64 id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_HANDLE, id);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  syncable::ModelType model_type = GetModelType();
  LOG_IF(WARNING, model_type == syncable::UNSPECIFIED ||
                  model_type == syncable::TOP_LEVEL_FOLDER)
      << "SyncAPI InitByIdLookup referencing unusual object.";
  return DecryptIfNecessary(entry_);
}

bool ReadNode::InitByClientTagLookup(syncable::ModelType model_type,
                                     const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return false;

  const std::string hash = GenerateSyncableHash(model_type, tag);

  entry_ = new syncable::Entry(transaction_->GetWrappedTrans(),
                               syncable::GET_BY_CLIENT_TAG, hash);
  return (entry_->good() && !entry_->Get(syncable::IS_DEL) &&
          DecryptIfNecessary(entry_));
}

const syncable::Entry* ReadNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* ReadNode::GetTransaction() const {
  return transaction_;
}

bool ReadNode::InitByTagLookup(const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return false;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_SERVER_TAG, tag);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  syncable::ModelType model_type = GetModelType();
  LOG_IF(WARNING, model_type == syncable::UNSPECIFIED ||
                  model_type == syncable::TOP_LEVEL_FOLDER)
      << "SyncAPI InitByTagLookup referencing unusually typed object.";
  return DecryptIfNecessary(entry_);
}

//////////////////////////////////////////////////////////////////////////
// ReadTransaction member definitions
ReadTransaction::ReadTransaction(UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL),
      close_transaction_(true) {
  transaction_ = new syncable::ReadTransaction(GetLookup(), __FILE__, __LINE__);
}

ReadTransaction::ReadTransaction(UserShare* share,
                                 syncable::BaseTransaction* trans)
    : BaseTransaction(share),
      transaction_(trans),
      close_transaction_(false) {}

ReadTransaction::~ReadTransaction() {
  if (close_transaction_) {
    delete transaction_;
  }
}

syncable::BaseTransaction* ReadTransaction::GetWrappedTrans() const {
  return transaction_;
}

//////////////////////////////////////////////////////////////////////////
// WriteTransaction member definitions
WriteTransaction::WriteTransaction(UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::WriteTransaction(GetLookup(), syncable::SYNCAPI,
                                                __FILE__, __LINE__);
}

WriteTransaction::~WriteTransaction() {
  delete transaction_;
}

syncable::BaseTransaction* WriteTransaction::GetWrappedTrans() const {
  return transaction_;
}

// A GaiaAuthenticator that uses HttpPostProviders instead of CURL.
class BridgedGaiaAuthenticator : public gaia::GaiaAuthenticator {
 public:
  BridgedGaiaAuthenticator(const string& user_agent, const string& service_id,
                           const string& gaia_url,
                           HttpPostProviderFactory* factory)
      : GaiaAuthenticator(user_agent, service_id, gaia_url),
        gaia_source_(user_agent), post_factory_(factory) {
  }

  virtual ~BridgedGaiaAuthenticator() {
  }

  virtual bool Post(const GURL& url, const string& post_body,
                    unsigned long* response_code, string* response_body) {
    string connection_url = "https://";
    connection_url += url.host();
    connection_url += url.path();
    HttpPostProviderInterface* http = post_factory_->Create();
    http->SetUserAgent(gaia_source_.c_str());
    // SSL is on 443 for Gaia Posts always.
    http->SetURL(connection_url.c_str(), kSSLPort);
    http->SetPostPayload("application/x-www-form-urlencoded",
                         post_body.length(), post_body.c_str());

    int os_error_code = 0;
    int int_response_code = 0;
    if (!http->MakeSynchronousPost(&os_error_code, &int_response_code)) {
      LOG(INFO) << "Http POST failed, error returns: " << os_error_code;
      return false;
    }
    *response_code = static_cast<int>(int_response_code);
    response_body->assign(http->GetResponseContent(),
                          http->GetResponseContentLength());
    post_factory_->Destroy(http);
    return true;
  }

  virtual int GetBackoffDelaySeconds(int current_backoff_delay) {
    return SyncerThread::GetRecommendedDelaySeconds(current_backoff_delay);
  }
 private:
  const std::string gaia_source_;
  scoped_ptr<HttpPostProviderFactory> post_factory_;
  DISALLOW_COPY_AND_ASSIGN(BridgedGaiaAuthenticator);
};

//////////////////////////////////////////////////////////////////////////
// SyncManager's implementation: SyncManager::SyncInternal
class SyncManager::SyncInternal
    : public net::NetworkChangeNotifier::Observer,
      public TalkMediator::Delegate,
      public browser_sync::ChannelEventHandler<syncable::DirectoryChangeEvent>,
      public browser_sync::ChannelEventHandler<SyncerEvent>{
  static const int kDefaultNudgeDelayMilliseconds;
  static const int kPreferencesNudgeDelayMilliseconds;
 public:
  explicit SyncInternal(SyncManager* sync_manager)
      : core_message_loop_(NULL),
        observer_(NULL),
        auth_problem_(AuthError::NONE),
        sync_manager_(sync_manager),
        registrar_(NULL),
        notification_pending_(false),
        initialized_(false),
        use_chrome_async_socket_(false),
        notification_method_(browser_sync::kDefaultNotificationMethod),
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  }

  ~SyncInternal() {
    DCHECK(!core_message_loop_);
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  }

  bool Init(const FilePath& database_location,
            const std::string& sync_server_and_path,
            int port,
            const char* gaia_service_id,
            const char* gaia_source,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            HttpPostProviderFactory* auth_post_factory,
            ModelSafeWorkerRegistrar* model_safe_worker_registrar,
            bool attempt_last_user_authentication,
            bool invalidate_last_user_auth_token,
            bool invalidate_xmpp_auth_token,
            const char* user_agent,
            const std::string& lsid,
            const bool use_chrome_async_socket,
            const bool try_ssltcp_first,
            browser_sync::NotificationMethod notification_method,
            const std::string& restored_key_for_bootstrapping);

  // Tell sync engine to submit credentials to GAIA for verification.
  // Successful GAIA authentication will kick off the following chain of events:
  // 1. Cause sync engine to open the syncer database.
  // 2. Trigger the AuthWatcher to create a Syncer for the directory and call
  //    SyncerThread::SyncDirectory; the SyncerThread will block until (4).
  // 3. Tell the ServerConnectionManager to pass the newly received GAIA auth
  //    token to a sync server to obtain a sync token.
  // 4. On receipt of this token, the ServerConnectionManager broadcasts
  //    a server-reachable event, which will unblock the SyncerThread.
  // 5. When StartSyncing is called, the Syncer will begin the sync process, by
  //    downloading from or uploading to the server.
  //
  // If authentication fails, an event will be broadcast all the way up to
  // the SyncManager::Observer. It may, in turn, decide to try again with new
  // credentials. Calling this method again is the appropriate course of action
  // to "retry".
  void Authenticate(const std::string& username, const std::string& password,
                    const std::string& captcha);

  // Tell the sync engine to start the syncing process.
  void StartSyncing();

  void SetPassphrase(const std::string& passphrase);

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  virtual void HandleChannelEvent(const syncable::DirectoryChangeEvent& event);
  void HandleTransactionEndingChangeEvent(
      const syncable::DirectoryChangeEvent& event);
  void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::DirectoryChangeEvent& event);
  void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::DirectoryChangeEvent& event);

  // This listener is called by the syncer channel for all syncer events.
  virtual void HandleChannelEvent(const SyncerEvent& event);

  // We have a direct hookup to the authwatcher to be notified for auth failures
  // on startup, to serve our UI needs.
  void HandleAuthWatcherEvent(const AuthWatcherEvent& event);

  // Listen here for directory opened events.
  void HandleDirectoryManagerEvent(
      const syncable::DirectoryManagerEvent& event);

  // Login to the talk mediator with the given credentials.
  void TalkMediatorLogin(
      const std::string& email, const std::string& token);

  // TalkMediator::Delegate implementation.

  virtual void OnNotificationStateChange(
      bool notifications_enabled);

  virtual void OnIncomingNotification(
      const IncomingNotificationData& notification_data);

  virtual void OnOutgoingNotification();

  // Accessors for the private members.
  DirectoryManager* dir_manager() { return share_.dir_manager.get(); }
  SyncAPIServerConnectionManager* connection_manager() {
    return connection_manager_.get();
  }
  SyncerThread* syncer_thread() { return syncer_thread_.get(); }
  TalkMediator* talk_mediator() { return talk_mediator_.get(); }
  AuthWatcher* auth_watcher() { return auth_watcher_.get(); }
  void set_observer(SyncManager::Observer* observer) { observer_ = observer; }
  UserShare* GetUserShare() { return &share_; }

  // Return the currently active (validated) username for use with syncable
  // types.
  const std::string& username_for_share() const {
    return share_.authenticated_name;
  }

  // Note about SyncManager::Status implementation: Status is a trimmed
  // down AllStatus::Status, augmented with authentication failure information
  // gathered from the internal AuthWatcher. The sync UI itself hooks up to
  // various sources like the AuthWatcher individually, but with syncapi we try
  // to keep everything status-related in one place. This means we have to
  // privately manage state about authentication failures, and whenever the
  // status or status summary is requested we aggregate this state with
  // AllStatus::Status information.
  Status ComputeAggregatedStatus();
  Status::Summary ComputeAggregatedStatusSummary();

  // See SyncManager::SetupForTestMode for information.
  void SetupForTestMode(const std::wstring& test_username);

  // See SyncManager::Shutdown for information.
  void Shutdown();

  // Whether we're initialized to the point of being able to accept changes
  // (and hence allow transaction creation). See initialized_ for details.
  bool initialized() const {
    AutoLock lock(initialized_mutex_);
    return initialized_;
  }

  void SetExtraChangeRecordData(int64 id,
                                syncable::ModelType type,
                                ChangeReorderBuffer* buffer,
                                const syncable::EntryKernel& original,
                                bool existed_before,
                                bool exists_now);

  // Called only by our NetworkChangeNotifier.
  virtual void OnIPAddressChanged();

  bool InitialSyncEndedForAllEnabledTypes() {
    syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
    if (!lookup.good()) {
      DCHECK(false) << "ScopedDirLookup failed when checking initial sync";
      return false;
    }

    ModelSafeRoutingInfo enabled_types;
    registrar_->GetModelSafeRoutingInfo(&enabled_types);
    for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
        i != enabled_types.end(); ++i) {
      if (!lookup->initial_sync_ended_for_type(i->first))
        return false;
    }
    return true;
  }

 private:
  // Try to authenticate using a LSID cookie.
  void AuthenticateWithLsid(const std::string& lsid);

  // Try to authenticate using persisted credentials from a previous successful
  // authentication. If no such credentials exist, calls OnAuthError on the
  // client to collect credentials. Otherwise, there exist local credentials
  // that were once used for a successful auth, so we'll try to re-use these.
  // Failure of that attempt will be communicated as normal using OnAuthError.
  // Since this entry point will bypass normal GAIA authentication and try to
  // authenticate directly with the sync service using a cached token,
  // authentication failure will generally occur due to expired credentials, or
  // possibly because of a password change.
  bool AuthenticateForUser(const std::string& username,
                           const std::string& auth_token);

  // Helper to call OnAuthError when no authentication credentials are
  // available.
  void RaiseAuthNeededEvent();

  // Helper to set initialized_ to true and raise an event to clients to notify
  // that initialization is complete and it is safe to send us changes. If
  // already initialized, this is a no-op.
  void MarkAndNotifyInitializationComplete();

  // If there's a pending notification to be sent, either from the
  // new_pending_notification flag or a previous unsuccessfully sent
  // notification, tries to send a notification.
  void SendPendingXMPPNotification(bool new_pending_notification);

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry stored in |a| and |b|.  Note that a node's index may
  // change without its NEXT_ID changing if the node at NEXT_ID also moved (but
  // the relative order is unchanged).  To handle such cases, we rely on the
  // caller to treat a position update on any sibling as updating the positions
  // of all siblings.
  static bool VisiblePositionsDiffer(const syncable::EntryKernel& a,
                                     const syncable::Entry& b) {
    // If the datatype isn't one where the browser model cares about position,
    // don't bother notifying that data model of position-only changes.
    if (!b.ShouldMaintainPosition())
      return false;
    if (a.ref(syncable::NEXT_ID) != b.Get(syncable::NEXT_ID))
      return true;
    if (a.ref(syncable::PARENT_ID) != b.Get(syncable::PARENT_ID))
      return true;
    return false;
  }

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  static bool VisiblePropertiesDiffer(const syncable::EntryKernel& a,
                                      const syncable::Entry& b) {
    syncable::ModelType model_type = b.GetModelType();
    // Suppress updates to items that aren't tracked by any browser model.
    if (model_type == syncable::UNSPECIFIED ||
        model_type == syncable::TOP_LEVEL_FOLDER) {
      return false;
    }
    if (a.ref(syncable::NON_UNIQUE_NAME) != b.Get(syncable::NON_UNIQUE_NAME))
      return true;
    if (a.ref(syncable::IS_DIR) != b.Get(syncable::IS_DIR))
      return true;
    if (a.ref(SPECIFICS).SerializeAsString() !=
        b.Get(SPECIFICS).SerializeAsString()) {
      return true;
    }
    if (VisiblePositionsDiffer(a, b))
      return true;
    return false;
  }

  bool ChangeBuffersAreEmpty() {
    for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
      if (!change_buffers_[i].IsEmpty())
        return false;
    }
    return true;
  }

  void CheckServerReachable() {
    if (connection_manager()) {
      connection_manager()->CheckServerReachable();
    } else {
      NOTREACHED() << "Should be valid connection manager!";
    }
  }

  // We couple the DirectoryManager and username together in a UserShare member
  // so we can return a handle to share_ to clients of the API for use when
  // constructing any transaction type.
  UserShare share_;

  // A wrapper around a sqlite store used for caching authentication data,
  // last user information, current sync-related URLs, and more.
  scoped_ptr<UserSettings> user_settings_;

  MessageLoop* core_message_loop_;

  // Observer registered via SetObserver/RemoveObserver.
  // WARNING: This can be NULL!
  SyncManager::Observer* observer_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  scoped_ptr<SyncAPIServerConnectionManager> connection_manager_;

  // The thread that runs the Syncer. Needs to be explicitly Start()ed.
  scoped_refptr<SyncerThread> syncer_thread_;

  // Notification (xmpp) handler.
  scoped_ptr<TalkMediator> talk_mediator_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // AuthWatcher kicks off the authentication process and follows it through
  // phase 1 (GAIA) to phase 2 (sync engine). As part of this work it determines
  // the initial connectivity and causes the server connection event to be
  // broadcast, which signals the syncer thread to start syncing.
  // It has a heavy duty constructor requiring boilerplate so we heap allocate.
  scoped_refptr<AuthWatcher> auth_watcher_;

  // Each element of this array is a store of change records produced by
  // HandleChangeEvent during the CALCULATE_CHANGES step.  The changes are
  // segregated by model type, and are stored here to be processed and
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING
  // step by HandleTransactionEndingChangeEvent.
  ChangeReorderBuffer change_buffers_[syncable::MODEL_TYPE_COUNT];

  // The event listener hookup that is registered for HandleChangeEvent.
  scoped_ptr<browser_sync::ChannelHookup<syncable::DirectoryChangeEvent> >
      dir_change_hookup_;

  // The event listener hookup registered for HandleSyncerEvent.
  scoped_ptr<browser_sync::ChannelHookup<SyncerEvent> > syncer_event_;

  // The event listener hookup registered for HandleAuthWatcherEvent.
  scoped_ptr<EventListenerHookup> authwatcher_hookup_;

  // The event listener hookup registered for the DirectoryManager (OPENED).
  scoped_ptr<EventListenerHookup> directory_manager_hookup_;

  // Our cache of a recent authentication problem. If no authentication problem
  // occurred, or if the last problem encountered has been cleared (by a
  // subsequent AuthWatcherEvent), this is set to NONE.
  AuthError::State auth_problem_;

  // The sync dir_manager to which we belong.
  SyncManager* const sync_manager_;

  // The entity that provides us with information about which types to sync.
  // The instance is shared between the SyncManager and the Syncer.
  ModelSafeWorkerRegistrar* registrar_;

  // True if the next SyncCycle should notify peers of an update.
  bool notification_pending_;

  // Set to true once Init has been called, and we know of an authenticated
  // valid) username either from a fresh authentication attempt (as in
  // first-use case) or from a previous attempt stored in our UserSettings
  // (as in the steady-state), and the syncable::Directory has been opened,
  // meaning we are ready to accept changes.  Protected by initialized_mutex_
  // as it can get read/set by both the SyncerThread and the AuthWatcherThread.
  bool initialized_;
  mutable Lock initialized_mutex_;

  bool use_chrome_async_socket_;
  browser_sync::NotificationMethod notification_method_;

  ScopedRunnableMethodFactory<SyncManager::SyncInternal> method_factory_;
};
const int SyncManager::SyncInternal::kDefaultNudgeDelayMilliseconds = 200;
const int SyncManager::SyncInternal::kPreferencesNudgeDelayMilliseconds = 2000;

SyncManager::SyncManager() {
  data_ = new SyncInternal(this);
}

bool SyncManager::Init(const FilePath& database_location,
                       const char* sync_server_and_path,
                       int sync_server_port,
                       const char* gaia_service_id,
                       const char* gaia_source,
                       bool use_ssl,
                       HttpPostProviderFactory* post_factory,
                       HttpPostProviderFactory* auth_post_factory,
                       ModelSafeWorkerRegistrar* registrar,
                       bool attempt_last_user_authentication,
                       bool invalidate_last_user_auth_token,
                       bool invalidate_xmpp_auth_token,
                       const char* user_agent,
                       const char* lsid,
                       bool use_chrome_async_socket,
                       bool try_ssltcp_first,
                       browser_sync::NotificationMethod notification_method,
                       const std::string& restored_key_for_bootstrapping) {
  DCHECK(post_factory);
  LOG(INFO) << "SyncManager starting Init...";
  string server_string(sync_server_and_path);
  return data_->Init(database_location,
                     server_string,
                     sync_server_port,
                     gaia_service_id,
                     gaia_source,
                     use_ssl,
                     post_factory,
                     auth_post_factory,
                     registrar,
                     attempt_last_user_authentication,
                     invalidate_last_user_auth_token,
                     invalidate_xmpp_auth_token,
                     user_agent,
                     lsid,
                     use_chrome_async_socket,
                     try_ssltcp_first,
                     notification_method,
                     restored_key_for_bootstrapping);
}

void SyncManager::Authenticate(const char* username, const char* password,
    const char* captcha) {
  data_->Authenticate(std::string(username), std::string(password),
                      std::string(captcha));
}

bool SyncManager::InitialSyncEndedForAllEnabledTypes() {
  return data_->InitialSyncEndedForAllEnabledTypes();
}

void SyncManager::StartSyncing() {
  data_->StartSyncing();
}

void SyncManager::SetPassphrase(const std::string& passphrase) {
  data_->SetPassphrase(passphrase);
}

bool SyncManager::RequestPause() {
  return data_->syncer_thread()->RequestPause();
}

bool SyncManager::RequestResume() {
  return data_->syncer_thread()->RequestResume();
}

void SyncManager::RequestNudge() {
  data_->syncer_thread()->NudgeSyncer(0, SyncerThread::kLocal);
}

const std::string& SyncManager::GetAuthenticatedUsername() {
  DCHECK(data_);
  return data_->username_for_share();
}

bool SyncManager::SyncInternal::Init(
    const FilePath& database_location,
    const std::string& sync_server_and_path,
    int port,
    const char* gaia_service_id,
    const char* gaia_source,
    bool use_ssl,
    HttpPostProviderFactory* post_factory,
    HttpPostProviderFactory* auth_post_factory,
    ModelSafeWorkerRegistrar* model_safe_worker_registrar,
    bool attempt_last_user_authentication,
    bool invalidate_last_user_auth_token,
    bool invalidate_xmpp_auth_token,
    const char* user_agent,
    const std::string& lsid,
    bool use_chrome_async_socket,
    bool try_ssltcp_first,
    browser_sync::NotificationMethod notification_method,
    const std::string& restored_key_for_bootstrapping) {

  LOG(INFO) << "Starting SyncInternal initialization.";

  core_message_loop_ = MessageLoop::current();
  DCHECK(core_message_loop_);
  notification_method_ = notification_method;
  // Set up UserSettings, creating the db if necessary. We need this to
  // instantiate a URLFactory to give to the Syncer.
  FilePath settings_db_file =
      database_location.Append(FilePath(kBookmarkSyncUserSettingsDatabase));
  user_settings_.reset(new UserSettings());
  if (!user_settings_->Init(settings_db_file))
    return false;

  registrar_ = model_safe_worker_registrar;

  LOG(INFO) << "Initialized sync user settings. Starting DirectoryManager.";

  share_.dir_manager.reset(new DirectoryManager(database_location));
  directory_manager_hookup_.reset(NewEventListenerHookup(
      share_.dir_manager->channel(), this,
          &SyncInternal::HandleDirectoryManagerEvent));
  share_.dir_manager->cryptographer()->Bootstrap(
      restored_key_for_bootstrapping);

  string client_id = user_settings_->GetClientId();
  connection_manager_.reset(new SyncAPIServerConnectionManager(
      sync_server_and_path, port, use_ssl, user_agent, client_id,
      post_factory));

  // Watch various objects for aggregated status.
  allstatus_.WatchConnectionManager(connection_manager());

  net::NetworkChangeNotifier::AddObserver(this);
  // TODO(akalin): CheckServerReachable() can block, which may cause jank if we
  // try to shut down sync.  Fix this.
  core_message_loop_->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&SyncInternal::CheckServerReachable));

  // NOTIFICATION_SERVER uses a substantially different notification method, so
  // it has its own MediatorThread implementation.  Everything else just uses
  // MediatorThreadImpl.
  notifier::MediatorThread* mediator_thread =
      (notification_method == browser_sync::NOTIFICATION_SERVER) ?
      new sync_notifier::ServerNotifierThread(use_chrome_async_socket,
                                              try_ssltcp_first) :
      new notifier::MediatorThreadImpl(use_chrome_async_socket,
                                       try_ssltcp_first);
  const bool kInitializeSsl = true;
  const bool kConnectImmediately = false;
  talk_mediator_.reset(new TalkMediatorImpl(mediator_thread, kInitializeSsl,
      kConnectImmediately, invalidate_xmpp_auth_token));
  if (notification_method != browser_sync::NOTIFICATION_LEGACY &&
      notification_method != browser_sync::NOTIFICATION_SERVER) {
    if (notification_method == browser_sync::NOTIFICATION_TRANSITIONAL) {
      talk_mediator_->AddSubscribedServiceUrl(
          browser_sync::kSyncLegacyServiceUrl);
    }
    talk_mediator_->AddSubscribedServiceUrl(browser_sync::kSyncServiceUrl);
  }

  // Listen to TalkMediator events ourselves
  talk_mediator_->SetDelegate(this);

  std::string gaia_url = gaia::kGaiaUrl;
  const char* service_id = gaia_service_id ?
      gaia_service_id : SYNC_SERVICE_NAME;

  BridgedGaiaAuthenticator* gaia_auth = new BridgedGaiaAuthenticator(
      gaia_source, service_id, gaia_url, auth_post_factory);

  LOG(INFO) << "Sync is bringing up authwatcher and SyncSessionContext.";

  auth_watcher_ = new AuthWatcher(dir_manager(),
                                  connection_manager(),
                                  gaia_source,
                                  service_id,
                                  gaia_url,
                                  user_settings_.get(),
                                  gaia_auth);

  authwatcher_hookup_.reset(NewEventListenerHookup(auth_watcher_->channel(),
      this, &SyncInternal::HandleAuthWatcherEvent));

  // Build a SyncSessionContext and store the worker in it.
  SyncSessionContext* context = new SyncSessionContext(
      connection_manager_.get(), auth_watcher(),
          dir_manager(), model_safe_worker_registrar);

  // The SyncerThread takes ownership of |context|.
  syncer_thread_ = new SyncerThread(context);
  allstatus_.WatchSyncerThread(syncer_thread());

  // Subscribe to the syncer thread's channel.
  syncer_event_.reset(syncer_thread()->relay_channel()->AddObserver(this));

  bool attempting_auth = false;
  std::string username, auth_token;
  if (attempt_last_user_authentication &&
      auth_watcher()->settings()->GetLastUserAndServiceToken(
          SYNC_SERVICE_NAME, &username, &auth_token)) {
    if (invalidate_last_user_auth_token) {
      auth_token += "bogus";
    }
    attempting_auth = AuthenticateForUser(username, auth_token);
  } else if (!lsid.empty()) {
    attempting_auth = true;
    AuthenticateWithLsid(lsid);
  }
  if (attempt_last_user_authentication && !attempting_auth)
    RaiseAuthNeededEvent();
  return true;
}

void SyncManager::SyncInternal::StartSyncing() {
  if (syncer_thread())  // NULL during certain unittests.
    syncer_thread()->Start();  // Start the syncer thread. This won't actually
                               // result in any syncing until at least the
                               // DirectoryManager broadcasts the OPENED event,
                               // and a valid server connection is detected.
}

void SyncManager::SyncInternal::MarkAndNotifyInitializationComplete() {
  // There is only one real time we need this mutex.  If we get an auth
  // success, and before the initial sync ends we get an auth failure.  In this
  // case we'll be listening to both the AuthWatcher and Syncer, and it's a race
  // between their respective threads to call MarkAndNotify.  We need to make
  // sure the observer is notified once and only once.
  {
    AutoLock lock(initialized_mutex_);
    if (initialized_)
      return;
    initialized_ = true;
  }

  // Notify that initialization is complete.
  if (observer_)
    observer_->OnInitializationComplete();
}

void SyncManager::SyncInternal::SendPendingXMPPNotification(
    bool new_pending_notification) {
  DCHECK_EQ(MessageLoop::current(), core_message_loop_);
  DCHECK_NE(notification_method_, browser_sync::NOTIFICATION_SERVER);
  notification_pending_ = notification_pending_ || new_pending_notification;
  if (!notification_pending_) {
    LOG(INFO) << "Not sending notification: no pending notification";
    return;
  }
  if (!talk_mediator_.get()) {
    LOG(INFO) << "Not sending notification: shutting down "
              << "(talk_mediator_ is NULL)";
    return;
  }
  LOG(INFO) << "Sending XMPP notification...";
  OutgoingNotificationData notification_data;
  if (notification_method_ == browser_sync::NOTIFICATION_LEGACY) {
    notification_data.service_id = browser_sync::kSyncLegacyServiceId;
    notification_data.service_url = browser_sync::kSyncLegacyServiceUrl;
    notification_data.send_content = false;
  } else {
    notification_data.service_id = browser_sync::kSyncServiceId;
    notification_data.service_url = browser_sync::kSyncServiceUrl;
    notification_data.send_content = true;
    notification_data.priority = browser_sync::kSyncPriority;
    notification_data.write_to_cache_only = true;
    if (notification_method_ == browser_sync::NOTIFICATION_NEW) {
      notification_data.service_specific_data =
          browser_sync::kSyncServiceSpecificData;
      notification_data.require_subscription = true;
    } else {
      notification_data.require_subscription = false;
    }
  }
  bool success = talk_mediator_->SendNotification(notification_data);
  if (success) {
    notification_pending_ = false;
    LOG(INFO) << "Sent XMPP notification";
  } else {
    LOG(INFO) << "Could not send XMPP notification";
  }
}

void SyncManager::SyncInternal::Authenticate(const std::string& username,
                                             const std::string& password,
                                             const std::string& captcha) {
  DCHECK(username_for_share().empty() || username == username_for_share())
        << "Username change from valid username detected";
  if (allstatus_.status().authenticated)
    return;
  if (password.empty()) {
    // TODO(timsteele): Seems like this shouldn't be needed, but auth_watcher
    // currently drops blank password attempts on the floor and doesn't update
    // state; it only LOGs an error in this case. We want to make sure we set
    // our GoogleServiceAuthError state to denote an error.
    RaiseAuthNeededEvent();
  }
  auth_watcher()->Authenticate(username, password, std::string(),
                               captcha);
}

void SyncManager::SyncInternal::AuthenticateWithLsid(const string& lsid) {
  DCHECK(!lsid.empty());
  auth_watcher()->AuthenticateWithLsid(lsid);
}

bool SyncManager::SyncInternal::AuthenticateForUser(
    const std::string& username, const std::string& auth_token) {
  share_.authenticated_name = username;

  // We optimize by opening the directory before the "fresh" authentication
  // attempt completes so that we can immediately begin processing changes.
  if (!dir_manager()->Open(username_for_share())) {
    if (observer_)
      observer_->OnStopSyncingPermanently();
    return false;
  }

  // Load the last-known good auth token into the connection manager and send
  // it off to the AuthWatcher for validation.  The result of the validation
  // will update the connection manager if necessary.
  connection_manager()->set_auth_token(auth_token);
  auth_watcher()->AuthenticateWithToken(username, auth_token);
  return true;
}

void SyncManager::SyncInternal::RaiseAuthNeededEvent() {
  auth_problem_ = AuthError::INVALID_GAIA_CREDENTIALS;
  if (observer_)
    observer_->OnAuthError(AuthError(auth_problem_));
}

void SyncManager::SyncInternal::SetPassphrase(
    const std::string& passphrase) {
  Cryptographer* cryptographer = dir_manager()->cryptographer();
  KeyParams params = {"localhost", "dummy", passphrase};
  if (cryptographer->has_pending_keys()) {
    if (!cryptographer->DecryptPendingKeys(params)) {
      observer_->OnPassphraseRequired();
      return;
    }
    // Nudge the syncer so that passwords updates that were waiting for this
    // passphrase get applied as soon as possible.
    sync_manager_->RequestNudge();
  } else {
    WriteTransaction trans(GetUserShare());
    WriteNode node(&trans);
    if (!node.InitByTagLookup(kNigoriTag)) {
      // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
      NOTREACHED();
      return;
    }
    cryptographer->AddKey(params);

    sync_pb::NigoriSpecifics specifics;
    cryptographer->GetKeys(specifics.mutable_encrypted());
    node.SetNigoriSpecifics(specifics);
  }

  std::string bootstrap_token;
  cryptographer->GetBootstrapToken(&bootstrap_token);
  observer_->OnPassphraseAccepted(bootstrap_token);
}

SyncManager::~SyncManager() {
  delete data_;
}

void SyncManager::SetObserver(Observer* observer) {
  data_->set_observer(observer);
}

void SyncManager::RemoveObserver() {
  data_->set_observer(NULL);
}

void SyncManager::Shutdown() {
  data_->Shutdown();
}

void SyncManager::SyncInternal::Shutdown() {
  method_factory_.RevokeAll();

  // We NULL out talk_mediator_ so that any tasks pumped below do not
  // trigger further XMPP actions.
  //
  // TODO(akalin): NULL the other member variables defensively, too.
  scoped_ptr<TalkMediator> talk_mediator(talk_mediator_.release());

  // First reset the AuthWatcher in case an auth attempt is in progress so that
  // it terminates gracefully before we shutdown and close other components.
  // Otherwise the attempt can complete after we've closed the directory, for
  // example, and cause initialization to continue, which is bad.
  if (auth_watcher_) {
    auth_watcher_->Shutdown();
    authwatcher_hookup_.reset();
  }

  if (syncer_thread()) {
    if (!syncer_thread()->Stop(kThreadExitTimeoutMsec)) {
      LOG(FATAL) << "Unable to stop the syncer, it won't be happy...";
    }
    syncer_event_.reset();
    syncer_thread_ = NULL;
  }

  // TODO(chron): Since the auth_watcher_ is held by the sync session state,
  //              we release the ref here after the syncer is deallocated.
  //              In reality the SyncerSessionState's pointer to the
  //              authwatcher should be ref counted, but for M6 we use this
  //              lower risk fix so it's deallocated on the original thread.
  if (auth_watcher_) {
    auth_watcher_ = NULL;
  }

  // Shutdown the xmpp buzz connection.
  if (talk_mediator.get()) {
    LOG(INFO) << "P2P: Mediator logout started.";
    talk_mediator->Logout();
    LOG(INFO) << "P2P: Mediator logout completed.";
    talk_mediator.reset();
    LOG(INFO) << "P2P: Mediator destroyed.";
  }

  // Pump any messages the auth watcher, syncer thread, or talk
  // mediator posted before they shut down. (See HandleSyncerEvent(),
  // HandleAuthWatcherEvent(), and HandleTalkMediatorEvent() for the
  // events that may be posted.)
  {
    CHECK(core_message_loop_);
    bool old_state = core_message_loop_->NestableTasksAllowed();
    core_message_loop_->SetNestableTasksAllowed(true);
    core_message_loop_->RunAllPending();
    core_message_loop_->SetNestableTasksAllowed(old_state);
  }

  net::NetworkChangeNotifier::RemoveObserver(this);

  if (dir_manager()) {
    dir_manager()->FinalSaveChangesForAll();
    dir_manager()->Close(username_for_share());
  }

  // Reset the DirectoryManager and UserSettings so they relinquish sqlite
  // handles to backing files.
  share_.dir_manager.reset();
  user_settings_.reset();

  // We don't want to process any more events.
  dir_change_hookup_.reset();

  core_message_loop_ = NULL;
}

void SyncManager::SyncInternal::OnIPAddressChanged() {
  LOG(INFO) << "IP address change detected";
  // TODO(akalin): CheckServerReachable() can block, which may cause
  // jank if we try to shut down sync.  Fix this.
  connection_manager()->CheckServerReachable();
}

void SyncManager::SyncInternal::HandleDirectoryManagerEvent(
    const syncable::DirectoryManagerEvent& event) {
  LOG(INFO) << "Sync internal handling a directory manager event";
  if (syncable::DirectoryManagerEvent::OPENED == event.what_happened) {
     DCHECK(!initialized()) << "Should only happen once";
     if (username_for_share().empty()) {
       share_.authenticated_name = event.dirname;
     }
     DCHECK(LowerCaseEqualsASCII(username_for_share(),
         StringToLowerASCII(event.dirname).c_str()))
         << "username_for_share= " << username_for_share()
         << ", event.dirname= " << event.dirname;
     MarkAndNotifyInitializationComplete();
  }
}

// Listen to model changes, filter out ones initiated by the sync API, and
// saves the rest (hopefully just backend Syncer changes resulting from
// ApplyUpdates) to data_->changelist.
void SyncManager::SyncInternal::HandleChannelEvent(
    const syncable::DirectoryChangeEvent& event) {
  if (event.todo == syncable::DirectoryChangeEvent::TRANSACTION_ENDING) {
    HandleTransactionEndingChangeEvent(event);
    return;
  } else if (event.todo == syncable::DirectoryChangeEvent::CALCULATE_CHANGES) {
    if (event.writer == syncable::SYNCAPI) {
      HandleCalculateChangesChangeEventFromSyncApi(event);
      return;
    }
    HandleCalculateChangesChangeEventFromSyncer(event);
    return;
  } else if (event.todo == syncable::DirectoryChangeEvent::SHUTDOWN) {
    dir_change_hookup_.reset();
  }
}

void SyncManager::SyncInternal::HandleTransactionEndingChangeEvent(
    const syncable::DirectoryChangeEvent& event) {
  // This notification happens immediately before a syncable WriteTransaction
  // falls out of scope. It happens while the channel mutex is still held,
  // and while the transaction mutex is held, so it cannot be re-entrant.
  DCHECK_EQ(event.todo, syncable::DirectoryChangeEvent::TRANSACTION_ENDING);
  if (!observer_ || ChangeBuffersAreEmpty())
    return;

  // This will continue the WriteTransaction using a read only wrapper.
  // This is the last chance for read to occur in the WriteTransaction
  // that's closing. This special ReadTransaction will not close the
  // underlying transaction.
  ReadTransaction trans(GetUserShare(), event.trans);

  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    if (change_buffers_[i].IsEmpty())
      continue;

    vector<ChangeRecord> ordered_changes;
    change_buffers_[i].GetAllChangesInTreeOrder(&trans, &ordered_changes);
    if (!ordered_changes.empty()) {
      observer_->OnChangesApplied(syncable::ModelTypeFromInt(i), &trans,
                                  &ordered_changes[0], ordered_changes.size());
    }
    change_buffers_[i].Clear();
  }
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncApi(
    const syncable::DirectoryChangeEvent& event) {
  // We have been notified about a user action changing the bookmark model.
  DCHECK_EQ(event.todo, syncable::DirectoryChangeEvent::CALCULATE_CHANGES);
  DCHECK(event.writer == syncable::SYNCAPI ||
         event.writer == syncable::UNITTEST);
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  bool exists_unsynced_items = false;
  bool only_preference_changes = true;
  for (syncable::OriginalEntries::const_iterator i = event.originals->begin();
       i != event.originals->end() && !exists_unsynced_items;
       ++i) {
    int64 id = i->ref(syncable::META_HANDLE);
    syncable::Entry e(event.trans, syncable::GET_BY_HANDLE, id);
    DCHECK(e.good());

    syncable::ModelType model_type = e.GetModelType();

    if (e.Get(syncable::IS_UNSYNCED)) {
      if (model_type == syncable::TOP_LEVEL_FOLDER ||
          model_type == syncable::UNSPECIFIED) {
        NOTREACHED() << "Permanent or underspecified item changed via syncapi.";
        continue;
      }
      // Unsynced items will cause us to nudge the the syncer.
      exists_unsynced_items = true;

      if (model_type != syncable::PREFERENCES)
        only_preference_changes = false;
    }
  }
  if (exists_unsynced_items && syncer_thread()) {
    int nudge_delay = only_preference_changes ?
        kPreferencesNudgeDelayMilliseconds : kDefaultNudgeDelayMilliseconds;
    syncer_thread()->NudgeSyncer(nudge_delay, SyncerThread::kLocal);
  }
}

void SyncManager::SyncInternal::SetExtraChangeRecordData(int64 id,
    syncable::ModelType type, ChangeReorderBuffer* buffer,
    const syncable::EntryKernel& original, bool existed_before,
    bool exists_now) {
  // If this is a deletion, attach the entity specifics as extra data
  // so that the delete can be processed.
  if (!exists_now && existed_before) {
    buffer->SetSpecificsForId(id, original.ref(SPECIFICS));
  }
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncer(
    const syncable::DirectoryChangeEvent& event) {
  // We only expect one notification per sync step, so change_buffers_ should
  // contain no pending entries.
  DCHECK_EQ(event.todo, syncable::DirectoryChangeEvent::CALCULATE_CHANGES);
  DCHECK(event.writer == syncable::SYNCER ||
         event.writer == syncable::UNITTEST);
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  for (syncable::OriginalEntries::const_iterator i = event.originals->begin();
       i != event.originals->end(); ++i) {
    int64 id = i->ref(syncable::META_HANDLE);
    syncable::Entry e(event.trans, syncable::GET_BY_HANDLE, id);
    bool existed_before = !i->ref(syncable::IS_DEL);
    bool exists_now = e.good() && !e.Get(syncable::IS_DEL);
    DCHECK(e.good());

    // Omit items that aren't associated with a model.
    syncable::ModelType type = e.GetModelType();
    if (type == syncable::TOP_LEVEL_FOLDER || type == syncable::UNSPECIFIED)
      continue;

    if (exists_now && !existed_before)
      change_buffers_[type].PushAddedItem(id);
    else if (!exists_now && existed_before)
      change_buffers_[type].PushDeletedItem(id);
    else if (exists_now && existed_before && VisiblePropertiesDiffer(*i, e))
      change_buffers_[type].PushUpdatedItem(id, VisiblePositionsDiffer(*i, e));

    SetExtraChangeRecordData(id, type, &change_buffers_[type], *i,
                             existed_before, exists_now);
  }
}

SyncManager::Status::Summary
SyncManager::SyncInternal::ComputeAggregatedStatusSummary() {
  switch (allstatus_.status().icon) {
    case AllStatus::OFFLINE:
      return Status::OFFLINE;
    case AllStatus::OFFLINE_UNSYNCED:
      return Status::OFFLINE_UNSYNCED;
    case AllStatus::SYNCING:
      return Status::SYNCING;
    case AllStatus::READY:
      return Status::READY;
    case AllStatus::CONFLICT:
      return Status::CONFLICT;
    case AllStatus::OFFLINE_UNUSABLE:
      return Status::OFFLINE_UNUSABLE;
    default:
      return Status::INVALID;
  }
}

SyncManager::Status SyncManager::SyncInternal::ComputeAggregatedStatus() {
  Status return_status =
      { ComputeAggregatedStatusSummary(),
        allstatus_.status().authenticated,
        allstatus_.status().server_up,
        allstatus_.status().server_reachable,
        allstatus_.status().server_broken,
        allstatus_.status().notifications_enabled,
        allstatus_.status().notifications_received,
        allstatus_.status().notifications_sent,
        allstatus_.status().unsynced_count,
        allstatus_.status().conflicting_count,
        allstatus_.status().syncing,
        allstatus_.status().initial_sync_ended,
        allstatus_.status().syncer_stuck,
        allstatus_.status().updates_available,
        allstatus_.status().updates_received,
        allstatus_.status().disk_full,
        false,   // TODO(ncarter): invalid store?
        allstatus_.status().max_consecutive_errors};
  return return_status;
}

void SyncManager::SyncInternal::HandleChannelEvent(const SyncerEvent& event) {
  if (!observer_)
    return;

  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncerEvent::SYNC_CYCLE_ENDED) {
    ModelSafeRoutingInfo enabled_types;
    registrar_->GetModelSafeRoutingInfo(&enabled_types);
    if (enabled_types.count(syncable::PASSWORDS) > 0) {
      Cryptographer* cryptographer =
          GetUserShare()->dir_manager->cryptographer();
      if (!cryptographer->is_ready() && !cryptographer->has_pending_keys()) {
        sync_api::ReadTransaction trans(GetUserShare());
        sync_api::ReadNode node(&trans);
        if (!node.InitByTagLookup(kNigoriTag)) {
          NOTREACHED();
          return;
        }
        const sync_pb::NigoriSpecifics& nigori = node.GetNigoriSpecifics();
        if (!nigori.encrypted().blob().empty()) {
          DCHECK(!cryptographer->CanDecrypt(nigori.encrypted()));
          cryptographer->SetPendingKeys(nigori.encrypted());
        }
      }

      // If we've completed a sync cycle and the cryptographer isn't ready yet,
      // prompt the user for a passphrase.
      if (!cryptographer->is_ready() || cryptographer->has_pending_keys()) {
        observer_->OnPassphraseRequired();
      }
    }

    if (!initialized())
      return;

    if (!event.snapshot->has_more_to_sync) {
      observer_->OnSyncCycleCompleted(event.snapshot);
    }

    if (notification_method_ != browser_sync::NOTIFICATION_SERVER) {
      // TODO(chron): Consider changing this back to track has_more_to_sync
      // only notify peers if a successful commit has occurred.
      bool new_pending_notification =
          (event.snapshot->syncer_status.num_successful_commits > 0);
      core_message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(
              this,
              &SyncManager::SyncInternal::SendPendingXMPPNotification,
              new_pending_notification));
    }
  }

  if (event.what_happened == SyncerEvent::PAUSED) {
    observer_->OnPaused();
    return;
  }

  if (event.what_happened == SyncerEvent::RESUMED) {
    observer_->OnResumed();
    return;
  }

  if (event.what_happened == SyncerEvent::STOP_SYNCING_PERMANENTLY) {
    observer_->OnStopSyncingPermanently();
    return;
  }
}

void SyncManager::SyncInternal::HandleAuthWatcherEvent(
    const AuthWatcherEvent& event) {
  allstatus_.HandleAuthWatcherEvent(event);
  // We don't care about an authentication attempt starting event, and we
  // don't want to reset our state to GoogleServiceAuthError::NONE because the
  // fact that an _attempt_ is starting doesn't change the fact that we have an
  // auth problem.
  if (event.what_happened == AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START)
    return;
  // We clear our last auth problem cache on new auth watcher events, and only
  // set it to indicate a problem state for certain AuthWatcherEvent types.
  auth_problem_ = AuthError::NONE;
  switch (event.what_happened) {
    case AuthWatcherEvent::AUTH_SUCCEEDED:
      DCHECK(!event.user_email.empty());
      // We now know the supplied username and password were valid. If this
      // wasn't the first sync, authenticated_name should already be assigned.
      if (username_for_share().empty()) {
        share_.authenticated_name = event.user_email;
      }

      DCHECK(LowerCaseEqualsASCII(username_for_share(),
          StringToLowerASCII(event.user_email).c_str()))
          << "username_for_share= " << username_for_share()
          << ", event.user_email= " << event.user_email;

      if (observer_)
        observer_->OnAuthError(AuthError::None());

      // Hook up the DirectoryChangeEvent listener, HandleChangeEvent.
      {
        syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
        if (!lookup.good()) {
          DCHECK(false) << "ScopedDirLookup creation failed; unable to hook "
                        << "up directory change event listener!";
          return;
        }

        // Note that we can end up here multiple times, for example if the
        // user had to re-login and we got a second AUTH_SUCCEEDED event. Take
        // care not to add ourselves as an observer a second time.
        if (!dir_change_hookup_.get())
          dir_change_hookup_.reset(lookup->AddChangeObserver(this));
      }

      if (!event.auth_token.empty()) {
        core_message_loop_->PostTask(
            FROM_HERE,
            NewRunnableMethod(
                this, &SyncManager::SyncInternal::TalkMediatorLogin,
                event.user_email, event.auth_token));
      }
      return;
    case AuthWatcherEvent::AUTH_RENEWED:
      DCHECK(!event.user_email.empty());
      DCHECK(!event.auth_token.empty());
      core_message_loop_->PostTask(
          FROM_HERE,
            NewRunnableMethod(
                this, &SyncManager::SyncInternal::TalkMediatorLogin,
                event.user_email, event.auth_token));
      return;
    // Authentication failures translate to GoogleServiceAuthError events.
    case AuthWatcherEvent::GAIA_AUTH_FAILED:     // Invalid GAIA credentials.
      if (event.auth_results->auth_error == gaia::CaptchaRequired) {
        auth_problem_ = AuthError::CAPTCHA_REQUIRED;
        std::string url_string("https://www.google.com/accounts/");
        url_string += event.auth_results->captcha_url;
        GURL captcha(url_string);
        observer_->OnAuthError(AuthError::FromCaptchaChallenge(
            event.auth_results->captcha_token, captcha,
            GURL(event.auth_results->auth_error_url)));
        return;
      } else if (event.auth_results->auth_error ==
                 gaia::ConnectionUnavailable) {
        auth_problem_ = AuthError::CONNECTION_FAILED;
      } else {
        auth_problem_ = AuthError::INVALID_GAIA_CREDENTIALS;
      }
      break;
    case AuthWatcherEvent::SERVICE_AUTH_FAILED:  // Expired GAIA credentials.
      auth_problem_ = AuthError::INVALID_GAIA_CREDENTIALS;
      break;
    case AuthWatcherEvent::SERVICE_USER_NOT_SIGNED_UP:
      auth_problem_ = AuthError::USER_NOT_SIGNED_UP;
      break;
    case AuthWatcherEvent::SERVICE_CONNECTION_FAILED:
      auth_problem_ = AuthError::CONNECTION_FAILED;
      break;
    default:  // We don't care about the many other AuthWatcherEvent types.
      return;
  }


  // Fire notification that the status changed due to an authentication error.
  if (observer_)
    observer_->OnAuthError(AuthError(auth_problem_));
}

void SyncManager::SyncInternal::OnNotificationStateChange(
    bool notifications_enabled) {
  LOG(INFO) << "P2P: Notifications enabled = "
            << (notifications_enabled ? "true" : "false");
  allstatus_.SetNotificationsEnabled(notifications_enabled);
  if (syncer_thread()) {
    syncer_thread()->SetNotificationsEnabled(notifications_enabled);
  }
  if ((notification_method_ != browser_sync::NOTIFICATION_SERVER) &&
      notifications_enabled) {
    // Nudge the syncer thread when notifications are enabled, in case there is
    // any data that has not yet been synced. If we are listening to
    // server-issued notifications, we are already guaranteed to receive a
    // notification on a successful connection.
    if (syncer_thread()) {
      syncer_thread()->NudgeSyncer(0, SyncerThread::kLocal);
    }

    // Send a notification as soon as subscriptions are on
    // (see http://code.google.com/p/chromium/issues/detail?id=38563 ).
    core_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &SyncManager::SyncInternal::SendPendingXMPPNotification,
            true));
  }
}

void SyncManager::SyncInternal::TalkMediatorLogin(
    const std::string& email, const std::string& token) {
  DCHECK_EQ(MessageLoop::current(), core_message_loop_);
  DCHECK(!email.empty());
  DCHECK(!token.empty());
  if (!talk_mediator_.get()) {
    LOG(INFO) << "Not logging in: shutting down "
              << "(talk_mediator_ is NULL)";
    return;
  }
  // TODO(akalin): Make talk_mediator automatically login on
  // auth token change.
  talk_mediator_->SetAuthToken(email, token, SYNC_SERVICE_NAME);
  talk_mediator_->Login();
}

void SyncManager::SyncInternal::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  // Check if the service url is a sync URL.  An empty service URL is
  // treated as a legacy sync notification.  If we're listening to
  // server-issued notifications, no need to check the service_url.
  if ((notification_method_ == browser_sync::NOTIFICATION_SERVER) ||
      notification_data.service_url.empty() ||
      (notification_data.service_url ==
       browser_sync::kSyncLegacyServiceUrl) ||
      (notification_data.service_url ==
       browser_sync::kSyncServiceUrl)) {
    LOG(INFO) << "P2P: Updates on server, pushing syncer";
    if (syncer_thread()) {
      syncer_thread()->NudgeSyncer(0, SyncerThread::kNotification);
    }
    allstatus_.IncrementNotificationsReceived();
  } else {
    LOG(WARNING) << "Notification fron unexpected source: "
                 << notification_data.service_url;
  }
}

void SyncManager::SyncInternal::OnOutgoingNotification() {
  DCHECK_NE(notification_method_, browser_sync::NOTIFICATION_SERVER);
  allstatus_.IncrementNotificationsSent();
}

SyncManager::Status::Summary SyncManager::GetStatusSummary() const {
  return data_->ComputeAggregatedStatusSummary();
}

SyncManager::Status SyncManager::GetDetailedStatus() const {
  return data_->ComputeAggregatedStatus();
}

SyncManager::SyncInternal* SyncManager::GetImpl() const { return data_; }

void SyncManager::SaveChanges() {
  data_->SaveChanges();
}

void SyncManager::SyncInternal::SaveChanges() {
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    DCHECK(false) << "ScopedDirLookup creation failed; Unable to SaveChanges";
    return;
  }
  lookup->SaveChanges();
}

void SyncManager::SetupForTestMode(const std::wstring& test_username) {
  DCHECK(data_) << "SetupForTestMode requires initialization";
  data_->SetupForTestMode(test_username);
}

void SyncManager::SyncInternal::SetupForTestMode(
    const std::wstring& test_username) {
  share_.authenticated_name = WideToUTF8(test_username);

  // Some tests are targeting only local db operations & integrity, and don't
  // want syncer thread interference.
  syncer_event_.reset();
  allstatus_.WatchSyncerThread(NULL);
  syncer_thread_ = NULL;

  if (!dir_manager()->Open(username_for_share()))
    DCHECK(false) << "Could not open directory when running in test mode";

  // Hook up the DirectoryChangeEvent listener, HandleChangeEvent.
  {
    syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
    if (!lookup.good()) {
      DCHECK(false) << "ScopedDirLookup creation failed; unable to hook "
                    << "up directory change event listener!";
      return;
    }
    dir_change_hookup_.reset(lookup->AddChangeObserver(this));
  }
}

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share)
    : lookup_(NULL) {
  DCHECK(share && share->dir_manager.get());
  lookup_ = new syncable::ScopedDirLookup(share->dir_manager.get(),
                                          share->authenticated_name);
  cryptographer_ = share->dir_manager->cryptographer();
  if (!(lookup_->good()))
    DCHECK(false) << "ScopedDirLookup failed on valid DirManager.";
}
BaseTransaction::~BaseTransaction() {
  delete lookup_;
}

UserShare* SyncManager::GetUserShare() const {
  DCHECK(data_->initialized()) << "GetUserShare requires initialization!";
  return data_->GetUserShare();
}

bool SyncManager::HasUnsyncedItems() const {
  sync_api::ReadTransaction trans(GetUserShare());
  return (trans.GetWrappedTrans()->directory()->unsynced_entity_count() != 0);
}

}  // namespace sync_api
