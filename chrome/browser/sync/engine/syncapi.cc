// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncapi.h"

#include <algorithm>
#include <bitset>
#include <iomanip>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/tracked.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/change_reorder_buffer.h"
#include "chrome/browser/sync/engine/http_post_provider_factory.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/nigori_util.h"
#include "chrome/browser/sync/engine/nudge_source.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/net/syncapi_server_connection_manager.h"
#include "chrome/browser/sync/engine/nudge_source.h"
#include "chrome/browser/sync/engine/sync_scheduler.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_backend.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_event_handler.h"
#include "chrome/browser/sync/js/js_reply_handler.h"
#include "chrome/browser/sync/js/js_sync_manager_observer.h"
#include "chrome/browser/sync/js/js_transaction_observer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/proto_value_conversions.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/syncable/directory_change_delegate.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/weak_handle.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/network_change_notifier.h"

using base::TimeDelta;
using browser_sync::AllStatus;
using browser_sync::Cryptographer;
using browser_sync::JsArgList;
using browser_sync::JsBackend;
using browser_sync::JsEventDetails;
using browser_sync::JsEventHandler;
using browser_sync::JsReplyHandler;
using browser_sync::JsSyncManagerObserver;
using browser_sync::JsTransactionObserver;
using browser_sync::MakeWeakHandle;
using browser_sync::WeakHandle;
using browser_sync::KeyParams;
using browser_sync::ModelSafeRoutingInfo;
using browser_sync::ModelSafeWorker;
using browser_sync::ModelSafeWorkerRegistrar;
using browser_sync::ServerConnectionEvent;
using browser_sync::ServerConnectionEventListener;
using browser_sync::SyncEngineEvent;
using browser_sync::SyncEngineEventListener;
using browser_sync::Syncer;
using browser_sync::SyncScheduler;
using browser_sync::kNigoriTag;
using browser_sync::sessions::SyncSessionContext;
using std::list;
using std::hex;
using std::string;
using std::vector;
using syncable::Directory;
using syncable::DirectoryManager;
using syncable::Entry;
using syncable::EntryKernelMutationSet;
using syncable::kEncryptedString;
using syncable::ModelType;
using syncable::ModelTypeBitSet;
using syncable::WriterTag;
using syncable::SPECIFICS;
using sync_pb::AutofillProfileSpecifics;

namespace {

typedef GoogleServiceAuthError AuthError;

static const int kThreadExitTimeoutMsec = 60000;
static const int kSSLPort = 443;
static const int kSyncSchedulerDelayMsec = 250;

#if defined(OS_CHROMEOS)
static const int kChromeOSNetworkChangeReactionDelayHackMsec = 5000;
#endif  // OS_CHROMEOS

}  // namespace

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
                                    std::string* out) {
  CHECK(out);
  int length_to_copy = server_name.length();
  if (IsNameServerIllegalAfterTrimming(server_name) &&
      EndsWithSpace(server_name)) {
    --length_to_copy;
  }
  *out = std::string(server_name.c_str(), length_to_copy);
}

// Compare the values of two EntitySpecifics, accounting for encryption.
static bool AreSpecificsEqual(const browser_sync::Cryptographer* cryptographer,
                              const sync_pb::EntitySpecifics& left,
                              const sync_pb::EntitySpecifics& right) {
  // Note that we can't compare encrypted strings directly as they are seeded
  // with a random value.
  std::string left_plaintext, right_plaintext;
  if (left.has_encrypted()) {
    if (!cryptographer->CanDecrypt(left.encrypted())) {
      NOTREACHED() << "Attempting to compare undecryptable data.";
      return false;
    }
    left_plaintext = cryptographer->DecryptToString(left.encrypted());
  } else {
    left_plaintext = left.SerializeAsString();
  }
  if (right.has_encrypted()) {
    if (!cryptographer->CanDecrypt(right.encrypted())) {
      NOTREACHED() << "Attempting to compare undecryptable data.";
      return false;
    }
    right_plaintext = cryptographer->DecryptToString(right.encrypted());
  } else {
    right_plaintext = right.SerializeAsString();
  }
  if (left_plaintext == right_plaintext) {
    return true;
  }
  return false;
}

// Helper function that converts a PassphraseRequiredReason value to a string.
std::string PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason) {
  switch (reason) {
    case REASON_PASSPHRASE_NOT_REQUIRED:
      return "REASON_PASSPHRASE_NOT_REQUIRED";
    case REASON_ENCRYPTION:
      return "REASON_ENCRYPTION";
    case REASON_DECRYPTION:
      return "REASON_DECRYPTION";
    case REASON_SET_PASSPHRASE_FAILED:
      return "REASON_SET_PASSPHRASE_FAILED";
    default:
      NOTREACHED();
      return "INVALID_REASON";
  }
}

// Helper function to determine if initial sync had ended for types.
bool InitialSyncEndedForTypes(syncable::ModelTypeSet types,
                              sync_api::UserShare* share) {
  syncable::ScopedDirLookup lookup(share->dir_manager.get(),
                                   share->name);
  if (!lookup.good()) {
    DCHECK(false) << "ScopedDirLookup failed when checking initial sync";
    return false;
  }

  for (syncable::ModelTypeSet::const_iterator i = types.begin();
       i != types.end(); ++i) {
    if (!lookup->initial_sync_ended_for_type(*i))
      return false;
  }
  return true;
}


UserShare::UserShare() {}

UserShare::~UserShare() {}

////////////////////////////////////
// BaseNode member definitions.

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

sync_pb::PasswordSpecificsData* DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics, Cryptographer* crypto) {
  if (!specifics.HasExtension(sync_pb::password))
    return NULL;
  const sync_pb::PasswordSpecifics& password_specifics =
      specifics.GetExtension(sync_pb::password);
  if (!password_specifics.has_encrypted())
    return NULL;
  const sync_pb::EncryptedData& encrypted = password_specifics.encrypted();
  scoped_ptr<sync_pb::PasswordSpecificsData> data(
      new sync_pb::PasswordSpecificsData);
  if (!crypto->Decrypt(encrypted, data.get()))
    return NULL;
  return data.release();
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

  // We assume any node with the encrypted field set has encrypted data.
  if (!specifics.has_encrypted())
    return true;

  const sync_pb::EncryptedData& encrypted =
      specifics.encrypted();
  std::string plaintext_data = GetTransaction()->GetCryptographer()->
      DecryptToString(encrypted);
  if (plaintext_data.length() == 0 ||
      !unencrypted_data_.ParseFromString(plaintext_data)) {
    LOG(ERROR) << "Failed to decrypt encrypted node of type " <<
      syncable::ModelTypeToString(GetModelType()) << ".";
    return false;
  }
  VLOG(2) << "Decrypted specifics of type "
          << syncable::ModelTypeToString(GetModelType())
          << " with content: " << plaintext_data;
  return true;
}

const sync_pb::EntitySpecifics& BaseNode::GetUnencryptedSpecifics(
    const syncable::Entry* entry) const {
  const sync_pb::EntitySpecifics& specifics = entry->Get(SPECIFICS);
  if (specifics.has_encrypted()) {
    DCHECK(syncable::GetModelTypeFromSpecifics(unencrypted_data_) !=
           syncable::UNSPECIFIED);
    return unencrypted_data_;
  } else {
    DCHECK(syncable::GetModelTypeFromSpecifics(unencrypted_data_) ==
           syncable::UNSPECIFIED);
    return specifics;
  }
}

int64 BaseNode::GetParentId() const {
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(),
                        GetEntry()->Get(syncable::PARENT_ID));
}

int64 BaseNode::GetId() const {
  return GetEntry()->Get(syncable::META_HANDLE);
}

int64 BaseNode::GetModificationTime() const {
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
  // TODO(akalin): Return time in a better format.
  node_info->SetString("modificationTime",
                       base::Int64ToString(GetModificationTime()));
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

////////////////////////////////////
// WriteNode member definitions
// Static.
bool WriteNode::UpdateEntryWithEncryption(
    browser_sync::Cryptographer* cryptographer,
    const sync_pb::EntitySpecifics& new_specifics,
    syncable::MutableEntry* entry) {
  syncable::ModelType type = syncable::GetModelTypeFromSpecifics(new_specifics);
  DCHECK_GE(type, syncable::FIRST_REAL_MODEL_TYPE);
  syncable::ModelTypeSet encrypted_types = cryptographer->GetEncryptedTypes();

  sync_pb::EntitySpecifics generated_specifics;
  if (type == syncable::PASSWORDS ||        // Has own encryption scheme.
      type == syncable::NIGORI ||           // Encrypted separately.
      encrypted_types.count(type) == 0 ||
      new_specifics.has_encrypted()) {
    // No encryption required.
    generated_specifics.CopyFrom(new_specifics);
  } else {
    // Encrypt new_specifics into generated_specifics.
    if (VLOG_IS_ON(2)) {
      scoped_ptr<DictionaryValue> value(entry->ToValue());
      std::string info;
      base::JSONWriter::Write(value.get(), true, &info);
      VLOG(2) << "Encrypting specifics of type "
              << syncable::ModelTypeToString(type)
              << " with content: "
              << info;
    }
    if (!cryptographer->is_initialized())
      return false;
    syncable::AddDefaultExtensionValue(type, &generated_specifics);
    if (!cryptographer->Encrypt(new_specifics,
                                generated_specifics.mutable_encrypted())) {
      NOTREACHED() << "Could not encrypt data for node of type "
                   << syncable::ModelTypeToString(type);
      return false;
    }
  }

  const sync_pb::EntitySpecifics& old_specifics = entry->Get(SPECIFICS);
  if (AreSpecificsEqual(cryptographer, old_specifics, generated_specifics)) {
    // Even if the data is the same but the old specifics are encrypted with an
    // old key, we should go ahead and re-encrypt with the new key.
    if ((!old_specifics.has_encrypted() &&
         !generated_specifics.has_encrypted()) ||
         cryptographer->CanDecryptUsingDefaultKey(old_specifics.encrypted())) {
      VLOG(2) << "Specifics of type " << syncable::ModelTypeToString(type)
              << " already match, dropping change.";
      return true;
    }
    // TODO(zea): Add some way to keep track of how often we're reencrypting
    // because of a passphrase change.
  }

  if (generated_specifics.has_encrypted()) {
    // Overwrite the possibly sensitive non-specifics data.
    entry->Put(syncable::NON_UNIQUE_NAME, kEncryptedString);
    // For bookmarks we actually put bogus data into the unencrypted specifics,
    // else the server will try to do it for us.
    if (type == syncable::BOOKMARKS) {
      sync_pb::BookmarkSpecifics* bookmark_specifics =
          generated_specifics.MutableExtension(sync_pb::bookmark);
      if (!entry->Get(syncable::IS_DIR))
        bookmark_specifics->set_url(kEncryptedString);
      bookmark_specifics->set_title(kEncryptedString);
    }
  }
  entry->Put(syncable::SPECIFICS, generated_specifics);
  syncable::MarkForSyncing(entry);
  return true;
}

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

  // Only set NON_UNIQUE_NAME to the title if we're not encrypted.
  if (GetEntitySpecifics().has_encrypted())
    entry_->Put(syncable::NON_UNIQUE_NAME, kEncryptedString);
  else
    entry_->Put(syncable::NON_UNIQUE_NAME, server_legal_name);

  // For bookmarks, we also set the title field in the specifics.
  // TODO(zea): refactor bookmarks to not need this functionality.
  if (GetModelType() == syncable::BOOKMARKS) {
    sync_pb::BookmarkSpecifics new_value = GetBookmarkSpecifics();
    new_value.set_title(server_legal_name);
    SetBookmarkSpecifics(new_value);  // Does it's own encryption checking.
  }

  MarkForSyncing();
}

void WriteNode::SetURL(const GURL& url) {
  sync_pb::BookmarkSpecifics new_value = GetBookmarkSpecifics();
  new_value.set_url(url.spec());
  SetBookmarkSpecifics(new_value);
}

void WriteNode::SetAppSpecifics(
    const sync_pb::AppSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::app)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetAutofillSpecifics(
    const sync_pb::AutofillSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::autofill)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetAutofillProfileSpecifics(
    const sync_pb::AutofillProfileSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::autofill_profile)->
      CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetBookmarkSpecifics(
    const sync_pb::BookmarkSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::bookmark)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetNigoriSpecifics(
    const sync_pb::NigoriSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::nigori)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetPasswordSpecifics(
    const sync_pb::PasswordSpecificsData& data) {
  DCHECK_EQ(syncable::PASSWORDS, GetModelType());

  Cryptographer* cryptographer = GetTransaction()->GetCryptographer();

  // Idempotency check to prevent unnecessary syncing: if the plaintexts match
  // and the old ciphertext is encrypted with the most current key, there's
  // nothing to do here.  Because each encryption is seeded with a different
  // random value, checking for equivalence post-encryption doesn't suffice.
  const sync_pb::EncryptedData& old_ciphertext =
      GetEntry()->Get(SPECIFICS).GetExtension(sync_pb::password).encrypted();
  scoped_ptr<sync_pb::PasswordSpecificsData> old_plaintext(
      DecryptPasswordSpecifics(GetEntry()->Get(SPECIFICS), cryptographer));
  if (old_plaintext.get() &&
      old_plaintext->SerializeAsString() == data.SerializeAsString() &&
      cryptographer->CanDecryptUsingDefaultKey(old_ciphertext)) {
    return;
  }

  sync_pb::PasswordSpecifics new_value;
  if (!cryptographer->Encrypt(data, new_value.mutable_encrypted())) {
    NOTREACHED() << "Failed to encrypt password, possibly due to sync node "
                 << "corruption";
    return;
  }

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::password)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetThemeSpecifics(
    const sync_pb::ThemeSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::theme)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetSessionSpecifics(
    const sync_pb::SessionSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::session)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetEntitySpecifics(
    const sync_pb::EntitySpecifics& new_value) {
  syncable::ModelType new_specifics_type =
      syncable::GetModelTypeFromSpecifics(new_value);
  DCHECK_NE(new_specifics_type, syncable::UNSPECIFIED);
  VLOG(1) << "Writing entity specifics of type "
          << syncable::ModelTypeToString(new_specifics_type);
  // GetModelType() can be unspecified if this is the first time this
  // node is being initialized (see PutModelType()).  Otherwise, it
  // should match |new_specifics_type|.
  if (GetModelType() != syncable::UNSPECIFIED) {
    DCHECK_EQ(new_specifics_type, GetModelType());
  }
  browser_sync::Cryptographer* cryptographer =
      GetTransaction()->GetCryptographer();

  // Preserve unknown fields.
  const sync_pb::EntitySpecifics& old_specifics = entry_->Get(SPECIFICS);
  sync_pb::EntitySpecifics new_specifics;
  new_specifics.CopyFrom(new_value);
  new_specifics.mutable_unknown_fields()->MergeFrom(
      old_specifics.unknown_fields());

  // Will update the entry if encryption was necessary.
  if (!UpdateEntryWithEncryption(cryptographer, new_specifics, entry_)) {
    return;
  }
  if (entry_->Get(SPECIFICS).has_encrypted()) {
    // EncryptIfNecessary already updated the entry for us and marked for
    // syncing if it was needed. Now we just make a copy of the unencrypted
    // specifics so that if this node is updated, we do not have to decrypt the
    // old data. Note that this only modifies the node's local data, not the
    // entry itself.
    SetUnencryptedSpecifics(new_value);
  }

  DCHECK_EQ(new_specifics_type, GetModelType());
}

void WriteNode::ResetFromSpecifics() {
  SetEntitySpecifics(GetEntitySpecifics());
}

void WriteNode::SetTypedUrlSpecifics(
    const sync_pb::TypedUrlSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::typed_url)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.MutableExtension(sync_pb::extension)->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
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
          DecryptIfNecessary());
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
          DecryptIfNecessary());
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
  DCHECK_EQ(syncable::NIGORI, model_type);
  return true;
}

void WriteNode::PutModelType(syncable::ModelType model_type) {
  // Set an empty specifics of the appropriate datatype.  The presence
  // of the specific extension will identify the model type.
  DCHECK(GetModelType() == model_type ||
         GetModelType() == syncable::UNSPECIFIED);  // Immutable once set.

  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultExtensionValue(model_type, &specifics);
  SetEntitySpecifics(specifics);
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

ReadNode::ReadNode() {
  entry_ = NULL;
  transaction_ = NULL;
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
  return DecryptIfNecessary();
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
          DecryptIfNecessary());
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
  return DecryptIfNecessary();
}

//////////////////////////////////////////////////////////////////////////
// ReadTransaction member definitions
ReadTransaction::ReadTransaction(const tracked_objects::Location& from_here,
                                 UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL),
      close_transaction_(true) {
  transaction_ = new syncable::ReadTransaction(from_here, GetLookup());
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
WriteTransaction::WriteTransaction(const tracked_objects::Location& from_here,
                                   UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::WriteTransaction(from_here, syncable::SYNCAPI,
                                                GetLookup());
}

WriteTransaction::~WriteTransaction() {
  delete transaction_;
}

syncable::BaseTransaction* WriteTransaction::GetWrappedTrans() const {
  return transaction_;
}

SyncManager::ChangeRecord::ChangeRecord()
    : id(kInvalidId), action(ACTION_ADD) {}

SyncManager::ChangeRecord::~ChangeRecord() {}

DictionaryValue* SyncManager::ChangeRecord::ToValue(
    const BaseTransaction* trans) const {
  DictionaryValue* value = new DictionaryValue();
  std::string action_str;
  switch (action) {
    case ACTION_ADD:
      action_str = "Add";
      break;
    case ACTION_DELETE:
      action_str = "Delete";
      break;
    case ACTION_UPDATE:
      action_str = "Update";
      break;
    default:
      NOTREACHED();
      action_str = "Unknown";
      break;
  }
  value->SetString("action", action_str);
  Value* node_value = NULL;
  if (action == ACTION_DELETE) {
    DictionaryValue* node_dict = new DictionaryValue();
    node_dict->SetString("id", base::Int64ToString(id));
    node_dict->Set("specifics",
                    browser_sync::EntitySpecificsToValue(specifics));
    if (extra.get()) {
      node_dict->Set("extra", extra->ToValue());
    }
    node_value = node_dict;
  } else {
    ReadNode node(trans);
    if (node.InitByIdLookup(id)) {
      node_value = node.GetDetailsAsValue();
    }
  }
  if (!node_value) {
    NOTREACHED();
    node_value = Value::CreateNullValue();
  }
  value->Set("node", node_value);
  return value;
}

SyncManager::ExtraPasswordChangeRecordData::ExtraPasswordChangeRecordData() {}

SyncManager::ExtraPasswordChangeRecordData::ExtraPasswordChangeRecordData(
    const sync_pb::PasswordSpecificsData& data)
    : unencrypted_(data) {
}

SyncManager::ExtraPasswordChangeRecordData::~ExtraPasswordChangeRecordData() {}

DictionaryValue* SyncManager::ExtraPasswordChangeRecordData::ToValue() const {
  return browser_sync::PasswordSpecificsDataToValue(unencrypted_);
}

const sync_pb::PasswordSpecificsData&
    SyncManager::ExtraPasswordChangeRecordData::unencrypted() const {
  return unencrypted_;
}

syncable::ModelTypeSet GetEncryptedTypes(
    const sync_api::BaseTransaction* trans) {
  Cryptographer* cryptographer = trans->GetCryptographer();
  return cryptographer->GetEncryptedTypes();
}

//////////////////////////////////////////////////////////////////////////
// SyncManager's implementation: SyncManager::SyncInternal
class SyncManager::SyncInternal
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public sync_notifier::SyncNotifierObserver,
      public JsBackend,
      public SyncEngineEventListener,
      public ServerConnectionEventListener,
      public syncable::DirectoryChangeDelegate {
  static const int kDefaultNudgeDelayMilliseconds;
  static const int kPreferencesNudgeDelayMilliseconds;
 public:
  explicit SyncInternal(const std::string& name)
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        registrar_(NULL),
        initialized_(false),
        setup_for_test_mode_(false),
        observing_ip_address_changes_(false) {
    // Pre-fill |notification_info_map_|.
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      notification_info_map_.insert(
          std::make_pair(syncable::ModelTypeFromInt(i), NotificationInfo()));
    }

    // Bind message handlers.
    BindJsMessageHandler(
        "getNotificationState",
        &SyncManager::SyncInternal::GetNotificationState);
    BindJsMessageHandler(
        "getNotificationInfo",
        &SyncManager::SyncInternal::GetNotificationInfo);
    BindJsMessageHandler(
        "getRootNodeDetails",
        &SyncManager::SyncInternal::GetRootNodeDetails);
    BindJsMessageHandler(
        "getNodeSummariesById",
        &SyncManager::SyncInternal::GetNodeSummariesById);
    BindJsMessageHandler(
        "getNodeDetailsById",
        &SyncManager::SyncInternal::GetNodeDetailsById);
    BindJsMessageHandler(
        "getChildNodeIds",
        &SyncManager::SyncInternal::GetChildNodeIds);
    BindJsMessageHandler(
        "findNodesContainingString",
        &SyncManager::SyncInternal::FindNodesContainingString);
  }

  virtual ~SyncInternal() {
    CHECK(!initialized_);
  }

  bool Init(const FilePath& database_location,
            const WeakHandle<JsEventHandler>& event_handler,
            const std::string& sync_server_and_path,
            int port,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            ModelSafeWorkerRegistrar* model_safe_worker_registrar,
            const std::string& user_agent,
            const SyncCredentials& credentials,
            sync_notifier::SyncNotifier* sync_notifier,
            const std::string& restored_key_for_bootstrapping,
            bool setup_for_test_mode);

  // Sign into sync with given credentials.
  // We do not verify the tokens given. After this call, the tokens are set
  // and the sync DB is open. True if successful, false if something
  // went wrong.
  bool SignIn(const SyncCredentials& credentials);

  // Update tokens that we're using in Sync. Email must stay the same.
  void UpdateCredentials(const SyncCredentials& credentials);

  // Called when the user disables or enables a sync type.
  void UpdateEnabledTypes();

  // Tell the sync engine to start the syncing process.
  void StartSyncingNormally();

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  bool IsUsingExplicitPassphrase();

  // Update the Cryptographer from the current nigori node.
  // Note: opens a transaction and can trigger an ON_PASSPHRASE_REQUIRED, so
  // should only be called after syncapi is fully initialized.
  // Returns true if cryptographer is ready, false otherwise.
  bool UpdateCryptographerFromNigori();

  // Set the datatypes we want to encrypt and encrypt any nodes as necessary.
  // Note: |encrypted_types| will be unioned with the current set of encrypted
  // types, as we do not currently support decrypting datatypes.
  void EncryptDataTypes(const syncable::ModelTypeSet& encrypted_types);

  // Try to set the current passphrase to |passphrase|, and record whether
  // it is an explicit passphrase or implicitly using gaia in the Nigori
  // node.
  void SetPassphrase(const std::string& passphrase, bool is_explicit);

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  virtual void HandleTransactionCompleteChangeEvent(
      const ModelTypeBitSet& models_with_changes);
  virtual ModelTypeBitSet HandleTransactionEndingChangeEvent(
      syncable::BaseTransaction* trans);
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const EntryKernelMutationSet& mutations,
      syncable::BaseTransaction* trans);
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const EntryKernelMutationSet& mutations,
      syncable::BaseTransaction* trans);

  // Listens for notifications from the ServerConnectionManager
  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  // Open the directory named with username_for_share
  bool OpenDirectory();

  // SyncNotifierObserver implementation.
  virtual void OnNotificationStateChange(
      bool notifications_enabled);

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads);

  virtual void StoreState(const std::string& cookie);

  // Thread-safe observers_ accessors.
  void CopyObservers(ObserverList<SyncManager::Observer>* observers_copy);
  bool HaveObservers() const;
  void AddObserver(SyncManager::Observer* observer);
  void RemoveObserver(SyncManager::Observer* observer);

  // Accessors for the private members.
  DirectoryManager* dir_manager() { return share_.dir_manager.get(); }
  SyncAPIServerConnectionManager* connection_manager() {
    return connection_manager_.get();
  }
  SyncScheduler* scheduler() { return scheduler_.get(); }
  UserShare* GetUserShare() {
    DCHECK(initialized_);
    return &share_;
  }

  // Return the currently active (validated) username for use with syncable
  // types.
  const std::string& username_for_share() const {
    return share_.name;
  }

  Status GetStatus();

  void RequestNudge(const tracked_objects::Location& nudge_location);

  void RequestNudgeForDataType(
      const tracked_objects::Location& nudge_location,
      const ModelType& type);

  void RequestEarlyExit();

  // See SyncManager::Shutdown for information.
  void Shutdown();

  // If this is a deletion for a password, sets the legacy
  // ExtraPasswordChangeRecordData field of |buffer|. Otherwise sets
  // |buffer|'s specifics field to contain the unencrypted data.
  void SetExtraChangeRecordData(int64 id,
                                syncable::ModelType type,
                                ChangeReorderBuffer* buffer,
                                Cryptographer* cryptographer,
                                const syncable::EntryKernel& original,
                                bool existed_before,
                                bool exists_now);

  // Called only by our NetworkChangeNotifier.
  virtual void OnIPAddressChanged();

  bool InitialSyncEndedForAllEnabledTypes() {
    syncable::ModelTypeSet types;
    ModelSafeRoutingInfo enabled_types;
    registrar_->GetModelSafeRoutingInfo(&enabled_types);
    for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
        i != enabled_types.end(); ++i) {
      types.insert(i->first);
    }

    return InitialSyncEndedForTypes(types, &share_);
  }

  // SyncEngineEventListener implementation.
  virtual void OnSyncEngineEvent(const SyncEngineEvent& event);

  // ServerConnectionEventListener implementation.
  virtual void OnServerConnectionEvent(const ServerConnectionEvent& event);

  // JsBackend implementation.
  virtual void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) OVERRIDE;
  virtual void ProcessJsMessage(
      const std::string& name, const JsArgList& args,
      const WeakHandle<JsReplyHandler>& reply_handler) OVERRIDE;

 private:
  struct NotificationInfo {
    int total_count;
    std::string payload;

    NotificationInfo() : total_count(0) {}

    ~NotificationInfo() {}

    // Returned pointer owned by the caller.
    DictionaryValue* ToValue() const {
      DictionaryValue* value = new DictionaryValue();
      value->SetInteger("totalCount", total_count);
      value->SetString("payload", payload);
      return value;
    }
  };

  typedef std::map<syncable::ModelType, NotificationInfo> NotificationInfoMap;
  typedef JsArgList
      (SyncManager::SyncInternal::*UnboundJsMessageHandler)(const JsArgList&);
  typedef base::Callback<JsArgList(JsArgList)> JsMessageHandler;
  typedef std::map<std::string, JsMessageHandler> JsMessageHandlerMap;

  // Helper to call OnAuthError when no authentication credentials are
  // available.
  void RaiseAuthNeededEvent();

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry stored in |a| and |b|.  Note that a node's index may
  // change without its NEXT_ID changing if the node at NEXT_ID also moved (but
  // the relative order is unchanged).  To handle such cases, we rely on the
  // caller to treat a position update on any sibling as updating the positions
  // of all siblings.
  static bool VisiblePositionsDiffer(
      const syncable::EntryKernelMutation& mutation) {
    const syncable::EntryKernel& a = mutation.original;
    const syncable::EntryKernel& b = mutation.mutated;
    // If the datatype isn't one where the browser model cares about position,
    // don't bother notifying that data model of position-only changes.
    if (!ShouldMaintainPosition(
            syncable::GetModelTypeFromSpecifics(b.ref(SPECIFICS))))
      return false;
    if (a.ref(syncable::NEXT_ID) != b.ref(syncable::NEXT_ID))
      return true;
    if (a.ref(syncable::PARENT_ID) != b.ref(syncable::PARENT_ID))
      return true;
    return false;
  }

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  static bool VisiblePropertiesDiffer(
      const syncable::EntryKernelMutation& mutation,
      Cryptographer* cryptographer) {
    const syncable::EntryKernel& a = mutation.original;
    const syncable::EntryKernel& b = mutation.mutated;
    const sync_pb::EntitySpecifics& a_specifics = a.ref(SPECIFICS);
    const sync_pb::EntitySpecifics& b_specifics = b.ref(SPECIFICS);
    DCHECK_EQ(syncable::GetModelTypeFromSpecifics(a_specifics),
              syncable::GetModelTypeFromSpecifics(b_specifics));
    syncable::ModelType model_type =
        syncable::GetModelTypeFromSpecifics(b_specifics);
    // Suppress updates to items that aren't tracked by any browser model.
    if (model_type < syncable::FIRST_REAL_MODEL_TYPE ||
        !a.ref(syncable::UNIQUE_SERVER_TAG).empty()) {
      return false;
    }
    if (a.ref(syncable::IS_DIR) != b.ref(syncable::IS_DIR))
      return true;
    if (!AreSpecificsEqual(cryptographer,
                           a.ref(syncable::SPECIFICS),
                           b.ref(syncable::SPECIFICS))) {
      return true;
    }
    // We only care if the name has changed if neither specifics is encrypted
    // (encrypted nodes blow away the NON_UNIQUE_NAME).
    if (!a_specifics.has_encrypted() && !b_specifics.has_encrypted() &&
        a.ref(syncable::NON_UNIQUE_NAME) != b.ref(syncable::NON_UNIQUE_NAME))
      return true;
    if (VisiblePositionsDiffer(mutation))
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

  void ReEncryptEverything(WriteTransaction* trans);

  // Initializes (bootstraps) the Cryptographer if NIGORI has finished
  // initial sync so that it can immediately start encrypting / decrypting.
  // If the restored key is incompatible with the current version of the NIGORI
  // node (which could happen if a restart occurred just after an update to
  // NIGORI was downloaded and the user must enter a new passphrase to decrypt)
  // then we will raise OnPassphraseRequired and set pending keys for
  // decryption.  Otherwise, the cryptographer is made ready (is_ready()).
  void BootstrapEncryption(const std::string& restored_key_for_bootstrapping);

  // Called for every notification. This updates the notification statistics
  // to be displayed in about:sync.
  void UpdateNotificationInfo(
      const syncable::ModelTypePayloadMap& type_payloads);

  // Checks for server reachabilty and requests a nudge.
  void OnIPAddressChangedImpl();

  // Helper function used only by the constructor.
  void BindJsMessageHandler(
    const std::string& name, UnboundJsMessageHandler unbound_message_handler);

  // Returned pointer is owned by the caller.
  static DictionaryValue* NotificationInfoToValue(
      const NotificationInfoMap& notification_info);

  // JS message handlers.
  JsArgList GetNotificationState(const JsArgList& args);
  JsArgList GetNotificationInfo(const JsArgList& args);
  JsArgList GetRootNodeDetails(const JsArgList& args);
  JsArgList GetNodeSummariesById(const JsArgList& args);
  JsArgList GetNodeDetailsById(const JsArgList& args);
  JsArgList GetChildNodeIds(const JsArgList& args);
  JsArgList FindNodesContainingString(const JsArgList& args);

  const std::string name_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SyncInternal> weak_ptr_factory_;

  // Thread-safe handle used by
  // HandleCalculateChangesChangeEventFromSyncApi(), which can be
  // called from any thread.  Valid only between between calls to
  // Init() and Shutdown().
  //
  // TODO(akalin): Ideally, we wouldn't need to store this; instead,
  // we'd have another worker class which implements
  // HandleCalculateChangesChangeEventFromSyncApi() and we'd pass it a
  // WeakHandle when we construct it.
  WeakHandle<SyncInternal> weak_handle_this_;

  // We couple the DirectoryManager and username together in a UserShare member
  // so we can return a handle to share_ to clients of the API for use when
  // constructing any transaction type.
  UserShare share_;

  // We have to lock around every observers_ access because it can get accessed
  // from any thread and added to/removed from on the core thread.
  mutable base::Lock observers_lock_;
  ObserverList<SyncManager::Observer> observers_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  scoped_ptr<SyncAPIServerConnectionManager> connection_manager_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  scoped_ptr<SyncScheduler> scheduler_;

  // The SyncNotifier which notifies us when updates need to be downloaded.
  scoped_ptr<sync_notifier::SyncNotifier> sync_notifier_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // Each element of this array is a store of change records produced by
  // HandleChangeEvent during the CALCULATE_CHANGES step.  The changes are
  // segregated by model type, and are stored here to be processed and
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING
  // step by HandleTransactionEndingChangeEvent. The list is cleared in the
  // TRANSACTION_COMPLETE step by HandleTransactionCompleteChangeEvent.
  ChangeReorderBuffer change_buffers_[syncable::MODEL_TYPE_COUNT];

  // The entity that provides us with information about which types to sync.
  // The instance is shared between the SyncManager and the Syncer.
  ModelSafeWorkerRegistrar* registrar_;

  // Set to true once Init has been called.
  bool initialized_;

  // True if the SyncManager should be running in test mode (no sync
  // scheduler actually communicating with the server).
  bool setup_for_test_mode_;

  // Whether we should respond to an IP address change notification.
  bool observing_ip_address_changes_;

  // Map used to store the notification info to be displayed in
  // about:sync page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsMessageHandlerMap js_message_handlers_;
  WeakHandle<JsEventHandler> js_event_handler_;
  JsSyncManagerObserver js_sync_manager_observer_;
  JsTransactionObserver js_transaction_observer_;
};
const int SyncManager::SyncInternal::kDefaultNudgeDelayMilliseconds = 200;
const int SyncManager::SyncInternal::kPreferencesNudgeDelayMilliseconds = 2000;

SyncManager::Observer::~Observer() {}

SyncManager::SyncManager(const std::string& name)
    : data_(new SyncInternal(name)) {}

SyncManager::Status::Status()
    : summary(INVALID),
      authenticated(false),
      server_up(false),
      server_reachable(false),
      server_broken(false),
      notifications_enabled(false),
      notifications_received(0),
      notifiable_commits(0),
      max_consecutive_errors(0),
      unsynced_count(0),
      conflicting_count(0),
      syncing(false),
      initial_sync_ended(false),
      syncer_stuck(false),
      updates_available(0),
      updates_received(0),
      tombstone_updates_received(0),
      disk_full(false),
      num_local_overwrites_total(0),
      num_server_overwrites_total(0),
      nonempty_get_updates(0),
      empty_get_updates(0),
      useless_sync_cycles(0),
      useful_sync_cycles(0),
      cryptographer_ready(false),
      crypto_has_pending_keys(false) {
}

SyncManager::Status::~Status() {
}

bool SyncManager::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int sync_server_port,
    bool use_ssl,
    HttpPostProviderFactory* post_factory,
    ModelSafeWorkerRegistrar* registrar,
    const std::string& user_agent,
    const SyncCredentials& credentials,
    sync_notifier::SyncNotifier* sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    bool setup_for_test_mode) {
  DCHECK(post_factory);
  VLOG(1) << "SyncManager starting Init...";
  string server_string(sync_server_and_path);
  return data_->Init(database_location,
                     event_handler,
                     server_string,
                     sync_server_port,
                     use_ssl,
                     post_factory,
                     registrar,
                     user_agent,
                     credentials,
                     sync_notifier,
                     restored_key_for_bootstrapping,
                     setup_for_test_mode);
}

void SyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  data_->UpdateCredentials(credentials);
}

void SyncManager::UpdateEnabledTypes() {
  data_->UpdateEnabledTypes();
}


bool SyncManager::InitialSyncEndedForAllEnabledTypes() {
  return data_->InitialSyncEndedForAllEnabledTypes();
}

void SyncManager::StartSyncingNormally() {
  data_->StartSyncingNormally();
}

void SyncManager::SetPassphrase(const std::string& passphrase,
     bool is_explicit) {
  data_->SetPassphrase(passphrase, is_explicit);
}

void SyncManager::EncryptDataTypes(
    const syncable::ModelTypeSet& encrypted_types) {
  data_->EncryptDataTypes(encrypted_types);
}

bool SyncManager::IsUsingExplicitPassphrase() {
  return data_ && data_->IsUsingExplicitPassphrase();
}

void SyncManager::RequestCleanupDisabledTypes() {
  if (data_->scheduler())
    data_->scheduler()->ScheduleCleanupDisabledTypes();
}

void SyncManager::RequestClearServerData() {
  if (data_->scheduler())
    data_->scheduler()->ScheduleClearUserData();
}

void SyncManager::RequestConfig(const syncable::ModelTypeBitSet& types,
    ConfigureReason reason) {
  if (!data_->scheduler()) {
    LOG(INFO)
        << "SyncManager::RequestConfig: bailing out because scheduler is "
        << "null";
    return;
  }
  StartConfigurationMode(NULL);
  data_->scheduler()->ScheduleConfig(types, reason);
}

void SyncManager::StartConfigurationMode(ModeChangeCallback* callback) {
  if (!data_->scheduler()) {
    LOG(INFO)
        << "SyncManager::StartConfigurationMode: could not start "
        << "configuration mode because because scheduler is null";
    return;
  }
  data_->scheduler()->Start(
      browser_sync::SyncScheduler::CONFIGURATION_MODE, callback);
}

const std::string& SyncManager::GetAuthenticatedUsername() {
  DCHECK(data_);
  return data_->username_for_share();
}

bool SyncManager::SyncInternal::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int port,
    bool use_ssl,
    HttpPostProviderFactory* post_factory,
    ModelSafeWorkerRegistrar* model_safe_worker_registrar,
    const std::string& user_agent,
    const SyncCredentials& credentials,
    sync_notifier::SyncNotifier* sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    bool setup_for_test_mode) {
  CHECK(!initialized_);

  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Starting SyncInternal initialization.";

  weak_handle_this_ = MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());

  registrar_ = model_safe_worker_registrar;
  setup_for_test_mode_ = setup_for_test_mode;

  sync_notifier_.reset(sync_notifier);

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(event_handler);

  share_.dir_manager.reset(new DirectoryManager(database_location));

  connection_manager_.reset(new SyncAPIServerConnectionManager(
      sync_server_and_path, port, use_ssl, user_agent, post_factory));

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  observing_ip_address_changes_ = true;

  connection_manager()->AddListener(this);

  // TODO(akalin): CheckServerReachable() can block, which may cause jank if we
  // try to shut down sync.  Fix this.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SyncInternal::CheckServerReachable,
                            weak_ptr_factory_.GetWeakPtr()));

  // Test mode does not use a syncer context or syncer thread.
  if (!setup_for_test_mode_) {
    // Build a SyncSessionContext and store the worker in it.
    VLOG(1) << "Sync is bringing up SyncSessionContext.";
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(&allstatus_);
    listeners.push_back(this);
    SyncSessionContext* context = new SyncSessionContext(
        connection_manager_.get(),
        dir_manager(),
        model_safe_worker_registrar,
        listeners);
    context->set_account_name(credentials.email);
    // The SyncScheduler takes ownership of |context|.
    scheduler_.reset(new SyncScheduler(name_, context, new Syncer()));
  }

  bool signed_in = SignIn(credentials);

  if (signed_in && scheduler()) {
    scheduler()->Start(
        browser_sync::SyncScheduler::CONFIGURATION_MODE, NULL);
  }

  initialized_ = true;

  // Notify that initialization is complete.
  ObserverList<SyncManager::Observer> temp_obs_list;
  CopyObservers(&temp_obs_list);
  FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                    OnInitializationComplete(
                        WeakHandle<JsBackend>(weak_ptr_factory_.GetWeakPtr())));

  // The following calls check that initialized_ is true.

  BootstrapEncryption(restored_key_for_bootstrapping);

  sync_notifier_->AddObserver(this);

  return signed_in;
}

void SyncManager::SyncInternal::BootstrapEncryption(
    const std::string& restored_key_for_bootstrapping) {
  // Cryptographer should only be accessed while holding a transaction.
  ReadTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();

  // Set the bootstrap token before bailing out if nigori node is not there.
  // This could happen if server asked us to migrate nigri.
  cryptographer->Bootstrap(restored_key_for_bootstrapping);
}

bool SyncManager::SyncInternal::UpdateCryptographerFromNigori() {
  DCHECK(initialized_);
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    NOTREACHED() << "BootstrapEncryption: lookup not good so bailing out";
    return false;
  }
  if (!lookup->initial_sync_ended_for_type(syncable::NIGORI))
    return false;  // Should only happen during first time sync.

  ReadTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();

  ReadNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    NOTREACHED();
    return false;
  }
  Cryptographer::UpdateResult result =
      cryptographer->Update(node.GetNigoriSpecifics());
  if (result == Cryptographer::NEEDS_PASSPHRASE) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnPassphraseRequired(sync_api::REASON_DECRYPTION));
  }

  allstatus_.SetCryptographerReady(cryptographer->is_ready());
  allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());

  return cryptographer->is_ready();
}

void SyncManager::SyncInternal::StartSyncingNormally() {
  // Start the sync scheduler. This won't actually result in any
  // syncing until at least the DirectoryManager broadcasts the OPENED
  // event, and a valid server connection is detected.
  if (scheduler())  // NULL during certain unittests.
    scheduler()->Start(SyncScheduler::NORMAL_MODE, NULL);
}

bool SyncManager::SyncInternal::OpenDirectory() {
  DCHECK(!initialized_) << "Should only happen once";

  bool share_opened = dir_manager()->Open(username_for_share(), this);
  DCHECK(share_opened);
  if (!share_opened) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnStopSyncingPermanently());

    LOG(ERROR) << "Could not open share for:" << username_for_share();
    return false;
  }

  // Database has to be initialized for the guid to be available.
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    NOTREACHED();
    return false;
  }

  connection_manager()->set_client_id(lookup->cache_guid());
  lookup->AddTransactionObserver(&js_transaction_observer_);
  return true;
}

bool SyncManager::SyncInternal::SignIn(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(share_.name.empty());
  share_.name = credentials.email;

  VLOG(1) << "Signing in user: " << username_for_share();
  if (!OpenDirectory())
    return false;

  // Retrieve and set the sync notifier state. This should be done
  // only after OpenDirectory is called.
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  std::string unique_id;
  std::string state;
  if (lookup.good()) {
    unique_id = lookup->cache_guid();
    state = lookup->GetNotificationState();
    VLOG(1) << "Read notification unique ID: " << unique_id;
    if (VLOG_IS_ON(1)) {
      std::string encoded_state;
      base::Base64Encode(state, &encoded_state);
      VLOG(1) << "Read notification state: " << encoded_state;
    }
  } else {
    LOG(ERROR) << "Could not read notification unique ID/state";
  }
  sync_notifier_->SetUniqueId(unique_id);
  sync_notifier_->SetState(state);

  UpdateCredentials(credentials);
  UpdateEnabledTypes();
  return true;
}

void SyncManager::SyncInternal::UpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(credentials.email, share_.name);
  DCHECK(!credentials.email.empty());
  DCHECK(!credentials.sync_token.empty());

  observing_ip_address_changes_ = true;
  if (connection_manager()->set_auth_token(credentials.sync_token)) {
    sync_notifier_->UpdateCredentials(
        credentials.email, credentials.sync_token);
    if (!setup_for_test_mode_) {
      CheckServerReachable();
    }
  }
}

void SyncManager::SyncInternal::UpdateEnabledTypes() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ModelSafeRoutingInfo routes;
  registrar_->GetModelSafeRoutingInfo(&routes);
  syncable::ModelTypeSet enabled_types;
  for (ModelSafeRoutingInfo::const_iterator it = routes.begin();
       it != routes.end(); ++it) {
    enabled_types.insert(it->first);
  }
  sync_notifier_->UpdateEnabledTypes(enabled_types);
}

void SyncManager::SyncInternal::RaiseAuthNeededEvent() {
  ObserverList<SyncManager::Observer> temp_obs_list;
  CopyObservers(&temp_obs_list);
  FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
      OnAuthError(AuthError(AuthError::INVALID_GAIA_CREDENTIALS)));
}

void SyncManager::SyncInternal::SetPassphrase(
    const std::string& passphrase, bool is_explicit) {
  // We do not accept empty passphrases.
  if (passphrase.empty()) {
    VLOG(1) << "Rejecting empty passphrase.";
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
        OnPassphraseRequired(sync_api::REASON_SET_PASSPHRASE_FAILED));
    return;
  }

  // All accesses to the cryptographer are protected by a transaction.
  WriteTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();
  KeyParams params = {"localhost", "dummy", passphrase};

  WriteNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return;
  }

  if (cryptographer->has_pending_keys()) {
    bool suceeded = false;

    // See if the explicit flag matches what is set in nigori. If not we dont
    // even try the passphrase. Note: This could mean that we wont try setting
    // the gaia password as passphrase if custom is elected by the user. Which
    // is fine because nigori node has all the old passwords in it.
    if (node.GetNigoriSpecifics().using_explicit_passphrase() == is_explicit) {
      if (cryptographer->DecryptPendingKeys(params)) {
        suceeded = true;
      } else {
        VLOG(1) << "Passphrase failed to decrypt pending keys.";
      }
    } else {
      VLOG(1) << "Not trying the passphrase because the explicit flags dont "
              << "match. Nigori node's explicit flag is "
              << node.GetNigoriSpecifics().using_explicit_passphrase();
    }

    if (!suceeded) {
      ObserverList<SyncManager::Observer> temp_obs_list;
      CopyObservers(&temp_obs_list);
      FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
          OnPassphraseRequired(sync_api::REASON_SET_PASSPHRASE_FAILED));
      return;
    }

    // Nudge the syncer so that encrypted datatype updates that were waiting for
    // this passphrase get applied as soon as possible.
    RequestNudge(FROM_HERE);
  } else {
    VLOG(1) << "No pending keys, adding provided passphrase.";

    // Prevent an implicit SetPassphrase request from changing an explicitly
    // set passphrase.
    if (!is_explicit && node.GetNigoriSpecifics().using_explicit_passphrase())
      return;

    cryptographer->AddKey(params);

    // TODO(tim): Bug 58231. It would be nice if SetPassphrase didn't require
    // messing with the Nigori node, because we can't call SetPassphrase until
    // download conditions are met vs Cryptographer init.  It seems like it's
    // safe to defer this work.
    sync_pb::NigoriSpecifics specifics(node.GetNigoriSpecifics());
    specifics.clear_encrypted();
    cryptographer->GetKeys(specifics.mutable_encrypted());
    specifics.set_using_explicit_passphrase(is_explicit);
    node.SetNigoriSpecifics(specifics);
    ReEncryptEverything(&trans);
  }

  VLOG(1) << "Passphrase accepted, bootstrapping encryption.";
  std::string bootstrap_token;
  cryptographer->GetBootstrapToken(&bootstrap_token);
  ObserverList<SyncManager::Observer> temp_obs_list;
  CopyObservers(&temp_obs_list);
  FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                    OnPassphraseAccepted(bootstrap_token));
}

bool SyncManager::SyncInternal::IsUsingExplicitPassphrase() {
  ReadTransaction trans(FROM_HERE, &share_);
  ReadNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return false;
  }

  return node.GetNigoriSpecifics().using_explicit_passphrase();
}

void SyncManager::SyncInternal::EncryptDataTypes(
    const syncable::ModelTypeSet& encrypted_types) {
  DCHECK(initialized_);
  VLOG(1) << "Attempting to encrypt datatypes "
          << syncable::ModelTypeSetToString(encrypted_types);

  WriteTransaction trans(FROM_HERE, GetUserShare());
  WriteNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    NOTREACHED() << "Unable to set encrypted datatypes because Nigori node not "
                 << "found.";
    return;
  }

  Cryptographer* cryptographer = trans.GetCryptographer();

  if (!cryptographer->is_initialized()) {
    VLOG(1) << "Attempting to encrypt datatypes when cryptographer not "
            << "initialized, prompting for passphrase.";
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    // TODO(zea): this isn't really decryption, but that's the only way we have
    // to prompt the user for a passsphrase. See http://crbug.com/91379.
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnPassphraseRequired(sync_api::REASON_DECRYPTION));
    return;
  }

  // Update the Nigori node's set of encrypted datatypes.
  // Note, we merge the current encrypted types with those requested. Once a
  // datatypes is marked as needing encryption, it is never unmarked.
  sync_pb::NigoriSpecifics nigori;
  nigori.CopyFrom(node.GetNigoriSpecifics());
  syncable::ModelTypeSet current_encrypted_types = GetEncryptedTypes(&trans);
  syncable::ModelTypeSet newly_encrypted_types;
  std::set_union(current_encrypted_types.begin(), current_encrypted_types.end(),
                 encrypted_types.begin(), encrypted_types.end(),
                 std::inserter(newly_encrypted_types,
                               newly_encrypted_types.begin()));
  allstatus_.SetEncryptedTypes(newly_encrypted_types);
  if (newly_encrypted_types == current_encrypted_types) {
    // Set of encrypted types has not changed, just notify and return.
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnEncryptionComplete(current_encrypted_types));
    return;
  }
  syncable::FillNigoriEncryptedTypes(newly_encrypted_types, &nigori);
  node.SetNigoriSpecifics(nigori);

  cryptographer->SetEncryptedTypes(nigori);

  // TODO(zea): only reencrypt this datatype? ReEncrypting everything is a
  // safer approach, and should not impact anything that is already encrypted
  // (redundant changes are ignored).
  ReEncryptEverything(&trans);
  return;
}

// TODO(zea): Add unit tests that ensure no sync changes are made when not
// needed.
void SyncManager::SyncInternal::ReEncryptEverything(WriteTransaction* trans) {
  syncable::ModelTypeSet encrypted_types =
      GetEncryptedTypes(trans);
  ModelSafeRoutingInfo routes;
  registrar_->GetModelSafeRoutingInfo(&routes);
  std::string tag;
  for (syncable::ModelTypeSet::iterator iter = encrypted_types.begin();
       iter != encrypted_types.end(); ++iter) {
    if (*iter == syncable::PASSWORDS || routes.count(*iter) == 0)
      continue;
    ReadNode type_root(trans);
    tag = syncable::ModelTypeToRootTag(*iter);
    if (!type_root.InitByTagLookup(tag)) {
      NOTREACHED();
      return;
    }

    // Iterate through all children of this datatype.
    std::queue<int64> to_visit;
    int64 child_id = type_root.GetFirstChildId();
    to_visit.push(child_id);
    while (!to_visit.empty()) {
      child_id = to_visit.front();
      to_visit.pop();
      if (child_id == kInvalidId)
        continue;

      WriteNode child(trans);
      if (!child.InitByIdLookup(child_id)) {
        NOTREACHED();
        continue;
      }
      if (child.GetIsFolder()) {
        to_visit.push(child.GetFirstChildId());
      }
      if (child.GetEntry()->Get(syncable::UNIQUE_SERVER_TAG).empty()) {
      // Rewrite the specifics of the node with encrypted data if necessary
      // (only rewrite the non-unique folders).
        child.ResetFromSpecifics();
      }
      to_visit.push(child.GetSuccessorId());
    }
  }

  if (routes.count(syncable::PASSWORDS) > 0) {
    // Passwords are encrypted with their own legacy scheme.
    ReadNode passwords_root(trans);
    std::string passwords_tag =
        syncable::ModelTypeToRootTag(syncable::PASSWORDS);
    // It's possible we'll have the password routing info and not the password
    // root if we attempted to SetPassphrase before passwords was enabled.
    if (passwords_root.InitByTagLookup(passwords_tag)) {
      int64 child_id = passwords_root.GetFirstChildId();
      while (child_id != kInvalidId) {
        WriteNode child(trans);
        if (!child.InitByIdLookup(child_id)) {
          NOTREACHED();
          return;
        }
        child.SetPasswordSpecifics(child.GetPasswordSpecifics());
        child_id = child.GetSuccessorId();
      }
    }
  }

  ObserverList<SyncManager::Observer> temp_obs_list;
  CopyObservers(&temp_obs_list);
  FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                    OnEncryptionComplete(encrypted_types));
}

SyncManager::~SyncManager() {
  delete data_;
}

void SyncManager::AddObserver(Observer* observer) {
  data_->AddObserver(observer);
}

void SyncManager::RemoveObserver(Observer* observer) {
  data_->RemoveObserver(observer);
}

void SyncManager::RequestEarlyExit() {
  data_->RequestEarlyExit();
}

void SyncManager::SyncInternal::RequestEarlyExit() {
  if (scheduler()) {
    scheduler()->RequestEarlyExit();
  }
}

void SyncManager::Shutdown() {
  data_->Shutdown();
}

void SyncManager::SyncInternal::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent any in-flight method calls from running.  Also
  // invalidates |weak_handle_this_|.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Automatically stops the scheduler.
  scheduler_.reset();

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  if (sync_notifier_.get()) {
    sync_notifier_->RemoveObserver(this);
  }
  sync_notifier_.reset();

  if (connection_manager_.get()) {
    connection_manager_->RemoveListener(this);
  }
  connection_manager_.reset();

  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  observing_ip_address_changes_ = false;

  if (dir_manager()) {
    syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
    if (lookup.good()) {
      lookup->RemoveTransactionObserver(&js_transaction_observer_);
    } else {
      NOTREACHED();
    }
    dir_manager()->FinalSaveChangesForAll();
    dir_manager()->Close(username_for_share());
  }

  // Reset the DirectoryManager and UserSettings so they relinquish sqlite
  // handles to backing files.
  share_.dir_manager.reset();

  setup_for_test_mode_ = false;
  registrar_ = NULL;

  initialized_ = false;

  // We reset this here, since only now we know it will not be
  // accessed from other threads (since we shut down everything).
  weak_handle_this_.Reset();
}

void SyncManager::SyncInternal::OnIPAddressChanged() {
  VLOG(1) << "IP address change detected";
  if (!observing_ip_address_changes_) {
    VLOG(1) << "IP address change dropped.";
    return;
  }

#if defined (OS_CHROMEOS)
  // TODO(tim): This is a hack to intentionally lose a race with flimflam at
  // shutdown, so we don't cause shutdown to wait for our http request.
  // http://crosbug.com/8429
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SyncInternal::OnIPAddressChangedImpl,
                 weak_ptr_factory_.GetWeakPtr()),
      kChromeOSNetworkChangeReactionDelayHackMsec);
#else
  OnIPAddressChangedImpl();
#endif  // defined(OS_CHROMEOS)
}

void SyncManager::SyncInternal::OnIPAddressChangedImpl() {
  // TODO(akalin): CheckServerReachable() can block, which may cause
  // jank if we try to shut down sync.  Fix this.
  connection_manager()->CheckServerReachable();
}

void SyncManager::SyncInternal::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  allstatus_.HandleServerConnectionEvent(event);
  if (event.connection_code ==
      browser_sync::HttpResponse::SERVER_CONNECTION_OK) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnAuthError(AuthError::None()));
  }

  if (event.connection_code == browser_sync::HttpResponse::SYNC_AUTH_ERROR) {
    observing_ip_address_changes_ = false;
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
        OnAuthError(AuthError(AuthError::INVALID_GAIA_CREDENTIALS)));
  }

  if (event.connection_code ==
      browser_sync::HttpResponse::SYNC_SERVER_ERROR) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
        OnAuthError(AuthError(AuthError::CONNECTION_FAILED)));
  }
}

void SyncManager::SyncInternal::HandleTransactionCompleteChangeEvent(
    const syncable::ModelTypeBitSet& models_with_changes) {
  // This notification happens immediately after the transaction mutex is
  // released. This allows work to be performed without blocking other threads
  // from acquiring a transaction.
  if (!HaveObservers())
    return;

  // Call commit.
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    if (models_with_changes.test(i)) {
      ObserverList<SyncManager::Observer> temp_obs_list;
      CopyObservers(&temp_obs_list);
      FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                        OnChangesComplete(syncable::ModelTypeFromInt(i)));
    }
  }
}

ModelTypeBitSet SyncManager::SyncInternal::HandleTransactionEndingChangeEvent(
    syncable::BaseTransaction* trans) {
  // This notification happens immediately before a syncable WriteTransaction
  // falls out of scope. It happens while the channel mutex is still held,
  // and while the transaction mutex is held, so it cannot be re-entrant.
  if (!HaveObservers() || ChangeBuffersAreEmpty())
    return ModelTypeBitSet();

  // This will continue the WriteTransaction using a read only wrapper.
  // This is the last chance for read to occur in the WriteTransaction
  // that's closing. This special ReadTransaction will not close the
  // underlying transaction.
  ReadTransaction read_trans(GetUserShare(), trans);

  syncable::ModelTypeBitSet models_with_changes;
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    if (change_buffers_[i].IsEmpty())
      continue;

    vector<ChangeRecord> ordered_changes;
    change_buffers_[i].GetAllChangesInTreeOrder(&read_trans, &ordered_changes);
    if (!ordered_changes.empty()) {
      ObserverList<SyncManager::Observer> temp_obs_list;
      CopyObservers(&temp_obs_list);
      FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
          OnChangesApplied(syncable::ModelTypeFromInt(i), &read_trans,
                           &ordered_changes[0], ordered_changes.size()));
      models_with_changes.set(i, true);
    }
    change_buffers_[i].Clear();
  }
  return models_with_changes;
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncApi(
    const EntryKernelMutationSet& mutations,
    syncable::BaseTransaction* trans) {
  if (!scheduler()) {
    return;
  }

  // We have been notified about a user action changing a sync model.
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  // The mutated model type, or UNSPECIFIED if nothing was mutated.
  syncable::ModelType mutated_model_type = syncable::UNSPECIFIED;

  // Find the first real mutation.  We assume that only a single model
  // type is mutated per transaction.
  for (syncable::EntryKernelMutationSet::const_iterator it =
           mutations.begin(); it != mutations.end(); ++it) {
    if (!it->mutated.ref(syncable::IS_UNSYNCED)) {
      continue;
    }

    syncable::ModelType model_type =
        syncable::GetModelTypeFromSpecifics(it->mutated.ref(SPECIFICS));
    if (model_type < syncable::FIRST_REAL_MODEL_TYPE) {
      NOTREACHED() << "Permanent or underspecified item changed via syncapi.";
      continue;
    }

    // Found real mutation.
    if (mutated_model_type == syncable::UNSPECIFIED) {
      mutated_model_type = model_type;
      break;
    }
  }

  // Nudge if necessary.
  if (mutated_model_type != syncable::UNSPECIFIED) {
    if (weak_handle_this_.IsInitialized()) {
      weak_handle_this_.Call(FROM_HERE,
                             &SyncInternal::RequestNudgeForDataType,
                             FROM_HERE,
                             mutated_model_type);
    } else {
      NOTREACHED();
    }
  }
}

void SyncManager::SyncInternal::SetExtraChangeRecordData(int64 id,
    syncable::ModelType type, ChangeReorderBuffer* buffer,
    Cryptographer* cryptographer, const syncable::EntryKernel& original,
    bool existed_before, bool exists_now) {
  // If this is a deletion and the datatype was encrypted, we need to decrypt it
  // and attach it to the buffer.
  if (!exists_now && existed_before) {
    sync_pb::EntitySpecifics original_specifics(original.ref(SPECIFICS));
    if (type == syncable::PASSWORDS) {
      // Passwords must use their own legacy ExtraPasswordChangeRecordData.
      scoped_ptr<sync_pb::PasswordSpecificsData> data(
          DecryptPasswordSpecifics(original_specifics, cryptographer));
      if (!data.get()) {
        NOTREACHED();
        return;
      }
      buffer->SetExtraDataForId(id, new ExtraPasswordChangeRecordData(*data));
    } else if (original_specifics.has_encrypted()) {
      // All other datatypes can just create a new unencrypted specifics and
      // attach it.
      const sync_pb::EncryptedData& encrypted = original_specifics.encrypted();
      if (!cryptographer->Decrypt(encrypted, &original_specifics)) {
        NOTREACHED();
        return;
      }
    }
    buffer->SetSpecificsForId(id, original_specifics);
  }
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncer(
    const EntryKernelMutationSet& mutations,
    syncable::BaseTransaction* trans) {
  // We only expect one notification per sync step, so change_buffers_ should
  // contain no pending entries.
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  Cryptographer* crypto = dir_manager()->GetCryptographer(trans);
  for (syncable::EntryKernelMutationSet::const_iterator it =
           mutations.begin(); it != mutations.end(); ++it) {
    bool existed_before = !it->original.ref(syncable::IS_DEL);
    bool exists_now = !it->mutated.ref(syncable::IS_DEL);

    // Omit items that aren't associated with a model.
    syncable::ModelType type =
        syncable::GetModelTypeFromSpecifics(it->mutated.ref(SPECIFICS));
    if (type < syncable::FIRST_REAL_MODEL_TYPE)
      continue;

    int64 id = it->original.ref(syncable::META_HANDLE);
    if (exists_now && !existed_before)
      change_buffers_[type].PushAddedItem(id);
    else if (!exists_now && existed_before)
      change_buffers_[type].PushDeletedItem(id);
    else if (exists_now && existed_before &&
             VisiblePropertiesDiffer(*it, crypto)) {
      change_buffers_[type].PushUpdatedItem(
          id, VisiblePositionsDiffer(*it));
    }

    SetExtraChangeRecordData(id, type, &change_buffers_[type], crypto,
                             it->original, existed_before, exists_now);
  }
}

SyncManager::Status SyncManager::SyncInternal::GetStatus() {
  return allstatus_.status();
}

void SyncManager::SyncInternal::RequestNudge(
    const tracked_objects::Location& location) {
  if (scheduler())
     scheduler()->ScheduleNudge(
        TimeDelta::FromMilliseconds(0), browser_sync::NUDGE_SOURCE_LOCAL,
        ModelTypeBitSet(), location);
}


void SyncManager::SyncInternal::RequestNudgeForDataType(
    const tracked_objects::Location& nudge_location,
    const ModelType& type) {
  if (!scheduler()) {
    NOTREACHED();
    return;
  }
  base::TimeDelta nudge_delay;
  switch (type) {
    case syncable::PREFERENCES:
      nudge_delay =
          TimeDelta::FromMilliseconds(kPreferencesNudgeDelayMilliseconds);
      break;
    case syncable::SESSIONS:
      nudge_delay = scheduler()->sessions_commit_delay();
      break;
    default:
      nudge_delay =
          TimeDelta::FromMilliseconds(kPreferencesNudgeDelayMilliseconds);
      break;
  }
  syncable::ModelTypeBitSet types;
  types.set(type);
  scheduler()->ScheduleNudge(nudge_delay,
                             browser_sync::NUDGE_SOURCE_LOCAL,
                             types,
                             nudge_location);
}

void SyncManager::SyncInternal::OnSyncEngineEvent(
    const SyncEngineEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!HaveObservers()) {
    LOG(INFO)
        << "OnSyncEngineEvent returning because observers_.size() is zero";
    return;
  }

  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    ModelSafeRoutingInfo enabled_types;
    registrar_->GetModelSafeRoutingInfo(&enabled_types);
    {
      // Check to see if we need to notify the frontend that we have newly
      // encrypted types or that we require a passphrase.
      sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
      Cryptographer* cryptographer = trans.GetCryptographer();
      // If we've completed a sync cycle and the cryptographer isn't ready
      // yet, prompt the user for a passphrase.
      if (cryptographer->has_pending_keys()) {
        VLOG(1) << "OnPassPhraseRequired Sent";
        ObserverList<SyncManager::Observer> temp_obs_list;
        CopyObservers(&temp_obs_list);
        FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                          OnPassphraseRequired(sync_api::REASON_DECRYPTION));
      } else if (!cryptographer->is_ready() &&
                 event.snapshot->initial_sync_ended.test(syncable::NIGORI)) {
        VLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
                << "ready";
        ObserverList<SyncManager::Observer> temp_obs_list;
        CopyObservers(&temp_obs_list);
        FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                          OnPassphraseRequired(sync_api::REASON_ENCRYPTION));
      }

      allstatus_.SetCryptographerReady(cryptographer->is_ready());
      allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());
      allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());

      // If everything is in order(we have the passphrase) then there is no
      // need to inform the listeners. They will just wait for sync
      // completion event and if no errors have been raised it means
      // encryption was succesful.
    }

    if (!initialized_) {
      LOG(INFO) << "OnSyncCycleCompleted not sent because sync api is not "
                << "initialized";
      return;
    }

    if (!event.snapshot->has_more_to_sync) {
      VLOG(1) << "OnSyncCycleCompleted sent";
      ObserverList<SyncManager::Observer> temp_obs_list;
      CopyObservers(&temp_obs_list);
      FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                        OnSyncCycleCompleted(event.snapshot));
    }

    // This is here for tests, which are still using p2p notifications.
    //
    // TODO(chron): Consider changing this back to track has_more_to_sync
    // only notify peers if a successful commit has occurred.
    bool is_notifiable_commit =
        (event.snapshot->syncer_status.num_successful_commits > 0);
    if (is_notifiable_commit) {
      allstatus_.IncrementNotifiableCommits();
      if (sync_notifier_.get()) {
        sync_notifier_->SendNotification();
      } else {
        VLOG(1) << "Not sending notification: sync_notifier_ is NULL";
      }
    }
  }

  if (event.what_happened == SyncEngineEvent::STOP_SYNCING_PERMANENTLY) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnStopSyncingPermanently());
    return;
  }

  if (event.what_happened == SyncEngineEvent::CLEAR_SERVER_DATA_SUCCEEDED) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnClearServerDataSucceeded());
    return;
  }

  if (event.what_happened == SyncEngineEvent::CLEAR_SERVER_DATA_FAILED) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnClearServerDataFailed());
    return;
  }

  if (event.what_happened == SyncEngineEvent::UPDATED_TOKEN) {
    ObserverList<SyncManager::Observer> temp_obs_list;
    CopyObservers(&temp_obs_list);
    FOR_EACH_OBSERVER(SyncManager::Observer, temp_obs_list,
                      OnUpdatedToken(event.updated_token));
    return;
  }
}

void SyncManager::SyncInternal::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_event_handler_ = event_handler;
  js_sync_manager_observer_.SetJsEventHandler(js_event_handler_);
  js_transaction_observer_.SetJsEventHandler(js_event_handler_);
}

void SyncManager::SyncInternal::ProcessJsMessage(
    const std::string& name, const JsArgList& args,
    const WeakHandle<JsReplyHandler>& reply_handler) {
  if (!initialized_) {
    NOTREACHED();
    return;
  }

  if (!reply_handler.IsInitialized()) {
    VLOG(1) << "Uninitialized reply handler; dropping unknown message "
            << name << " with args " << args.ToString();
    return;
  }

  JsMessageHandler js_message_handler = js_message_handlers_[name];
  if (js_message_handler.is_null()) {
    VLOG(1) << "Dropping unknown message " << name
              << " with args " << args.ToString();
    return;
  }

  reply_handler.Call(FROM_HERE,
                     &JsReplyHandler::HandleJsReply,
                     name, js_message_handler.Run(args));
}

void SyncManager::SyncInternal::BindJsMessageHandler(
    const std::string& name,
    UnboundJsMessageHandler unbound_message_handler) {
  js_message_handlers_[name] =
      base::Bind(unbound_message_handler, base::Unretained(this));
}

DictionaryValue* SyncManager::SyncInternal::NotificationInfoToValue(
    const NotificationInfoMap& notification_info) {
  DictionaryValue* value = new DictionaryValue();

  for (NotificationInfoMap::const_iterator it = notification_info.begin();
      it != notification_info.end(); ++it) {
    const std::string& model_type_str =
        syncable::ModelTypeToString(it->first);
    value->Set(model_type_str, it->second.ToValue());
  }

  return value;
}

JsArgList SyncManager::SyncInternal::GetNotificationState(
    const JsArgList& args) {
  bool notifications_enabled = allstatus_.status().notifications_enabled;
  ListValue return_args;
  return_args.Append(Value::CreateBooleanValue(notifications_enabled));
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetNotificationInfo(
    const JsArgList& args) {
  ListValue return_args;
  return_args.Append(NotificationInfoToValue(notification_info_map_));
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetRootNodeDetails(
    const JsArgList& args) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode root(&trans);
  root.InitByRootLookup();
  ListValue return_args;
  return_args.Append(root.GetDetailsAsValue());
  return JsArgList(&return_args);
}

namespace {

int64 GetId(const ListValue& ids, int i) {
  std::string id_str;
  if (!ids.GetString(i, &id_str)) {
    return kInvalidId;
  }
  int64 id = kInvalidId;
  if (!base::StringToInt64(id_str, &id)) {
    return kInvalidId;
  }
  return id;
}

JsArgList GetNodeInfoById(const JsArgList& args,
                          UserShare* user_share,
                          DictionaryValue* (BaseNode::*info_getter)() const) {
  CHECK(info_getter);
  ListValue return_args;
  ListValue* node_summaries = new ListValue();
  return_args.Append(node_summaries);
  ListValue* id_list = NULL;
  ReadTransaction trans(FROM_HERE, user_share);
  if (args.Get().GetList(0, &id_list)) {
    CHECK(id_list);
    for (size_t i = 0; i < id_list->GetSize(); ++i) {
      int64 id = GetId(*id_list, i);
      if (id == kInvalidId) {
        continue;
      }
      ReadNode node(&trans);
      if (!node.InitByIdLookup(id)) {
        continue;
      }
      node_summaries->Append((node.*info_getter)());
    }
  }
  return JsArgList(&return_args);
}

}  // namespace

JsArgList SyncManager::SyncInternal::GetNodeSummariesById(
    const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetSummaryAsValue);
}

JsArgList SyncManager::SyncInternal::GetNodeDetailsById(
    const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetDetailsAsValue);
}

JsArgList SyncManager::SyncInternal::GetChildNodeIds(
    const JsArgList& args) {
  ListValue return_args;
  ListValue* child_ids = new ListValue();
  return_args.Append(child_ids);
  int64 id = GetId(args.Get(), 0);
  if (id != kInvalidId) {
    ReadTransaction trans(FROM_HERE, GetUserShare());
    syncable::Directory::ChildHandles child_handles;
    trans.GetLookup()->GetChildHandlesByHandle(trans.GetWrappedTrans(),
                                               id, &child_handles);
    for (syncable::Directory::ChildHandles::const_iterator it =
             child_handles.begin(); it != child_handles.end(); ++it) {
      child_ids->Append(Value::CreateStringValue(
          base::Int64ToString(*it)));
    }
  }
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::FindNodesContainingString(
    const JsArgList& args) {
  std::string query;
  ListValue return_args;
  if (!args.Get().GetString(0, &query)) {
    return_args.Append(new ListValue());
    return JsArgList(&return_args);
  }

  // Convert the query string to lower case to perform case insensitive
  // searches.
  std::string lowercase_query = query;
  StringToLowerASCII(&lowercase_query);

  ListValue* result = new ListValue();
  return_args.Append(result);

  ReadTransaction trans(FROM_HERE, GetUserShare());
  std::vector<const syncable::EntryKernel*> entry_kernels;
  trans.GetLookup()->GetAllEntryKernels(trans.GetWrappedTrans(),
                                        &entry_kernels);

  for (std::vector<const syncable::EntryKernel*>::const_iterator it =
           entry_kernels.begin(); it != entry_kernels.end(); ++it) {
    if ((*it)->ContainsString(lowercase_query)) {
      result->Append(new StringValue(base::Int64ToString(
          (*it)->ref(syncable::META_HANDLE))));
    }
  }

  return JsArgList(&return_args);
}

void SyncManager::SyncInternal::OnNotificationStateChange(
    bool notifications_enabled) {
  VLOG(1) << "P2P: Notifications enabled = "
          << (notifications_enabled ? "true" : "false");
  allstatus_.SetNotificationsEnabled(notifications_enabled);
  if (scheduler()) {
    scheduler()->set_notifications_enabled(notifications_enabled);
  }
  if (js_event_handler_.IsInitialized()) {
    DictionaryValue details;
    details.Set("enabled", Value::CreateBooleanValue(notifications_enabled));
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onNotificationStateChange",
                           JsEventDetails(&details));
  }
}

void SyncManager::SyncInternal::UpdateNotificationInfo(
    const syncable::ModelTypePayloadMap& type_payloads) {
  for (syncable::ModelTypePayloadMap::const_iterator it = type_payloads.begin();
       it != type_payloads.end(); ++it) {
    NotificationInfo* info = &notification_info_map_[it->first];
    info->total_count++;
    info->payload = it->second;
  }
}

void SyncManager::SyncInternal::OnIncomingNotification(
    const syncable::ModelTypePayloadMap& type_payloads) {
  if (!type_payloads.empty()) {
    if (scheduler()) {
      scheduler()->ScheduleNudgeWithPayloads(
          TimeDelta::FromMilliseconds(kSyncSchedulerDelayMsec),
          browser_sync::NUDGE_SOURCE_NOTIFICATION,
          type_payloads, FROM_HERE);
    }
    allstatus_.IncrementNotificationsReceived();
    UpdateNotificationInfo(type_payloads);
  } else {
    LOG(WARNING) << "Sync received notification without any type information.";
  }

  if (js_event_handler_.IsInitialized()) {
    DictionaryValue details;
    ListValue* changed_types = new ListValue();
    details.Set("changedTypes", changed_types);
    for (syncable::ModelTypePayloadMap::const_iterator
             it = type_payloads.begin();
         it != type_payloads.end(); ++it) {
      const std::string& model_type_str =
          syncable::ModelTypeToString(it->first);
      changed_types->Append(Value::CreateStringValue(model_type_str));
    }
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onIncomingNotification",
                           JsEventDetails(&details));
  }
}

void SyncManager::SyncInternal::StoreState(
    const std::string& state) {
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    LOG(ERROR) << "Could not write notification state";
    // TODO(akalin): Propagate result callback all the way to this
    // function and call it with "false" to signal failure.
    return;
  }
  if (VLOG_IS_ON(1)) {
    std::string encoded_state;
    base::Base64Encode(state, &encoded_state);
    VLOG(1) << "Writing notification state: " << encoded_state;
  }
  lookup->SetNotificationState(state);
  lookup->SaveChanges();
}


// Note: it is possible that an observer will remove itself after we have made
// a copy, but before the copy is consumed. This could theoretically result
// in accessing a garbage pointer, but can only occur when an about:sync window
// is closed in the middle of a notification.
// See crbug.com/85481.
void SyncManager::SyncInternal::CopyObservers(
    ObserverList<SyncManager::Observer>* observers_copy) {
  DCHECK_EQ(0U, observers_copy->size());
  base::AutoLock lock(observers_lock_);
  if (observers_.size() == 0)
    return;
  ObserverListBase<SyncManager::Observer>::Iterator it(observers_);
  SyncManager::Observer* obs;
  while ((obs = it.GetNext()) != NULL)
    observers_copy->AddObserver(obs);
}

bool SyncManager::SyncInternal::HaveObservers() const {
  base::AutoLock lock(observers_lock_);
  return observers_.size() > 0;
}

void SyncManager::SyncInternal::AddObserver(
    SyncManager::Observer* observer) {
  base::AutoLock lock(observers_lock_);
  observers_.AddObserver(observer);
}

void SyncManager::SyncInternal::RemoveObserver(
    SyncManager::Observer* observer) {
  base::AutoLock lock(observers_lock_);
  observers_.RemoveObserver(observer);
}

SyncManager::Status::Summary SyncManager::GetStatusSummary() const {
  return data_->GetStatus().summary;
}

SyncManager::Status SyncManager::GetDetailedStatus() const {
  return data_->GetStatus();
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

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share)
    : lookup_(NULL) {
  DCHECK(share && share->dir_manager.get());
  lookup_ = new syncable::ScopedDirLookup(share->dir_manager.get(),
                                          share->name);
  cryptographer_ = share->dir_manager->GetCryptographer(this);
  if (!(lookup_->good()))
    DCHECK(false) << "ScopedDirLookup failed on valid DirManager.";
}
BaseTransaction::~BaseTransaction() {
  delete lookup_;
}

UserShare* SyncManager::GetUserShare() const {
  return data_->GetUserShare();
}

void SyncManager::RefreshEncryption() {
  if (data_->UpdateCryptographerFromNigori())
    data_->EncryptDataTypes(syncable::ModelTypeSet());
}

syncable::ModelTypeSet SyncManager::GetEncryptedDataTypes() const {
  sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
  return GetEncryptedTypes(&trans);
}

bool SyncManager::HasUnsyncedItems() const {
  sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
  return (trans.GetWrappedTrans()->directory()->unsynced_entity_count() != 0);
}

void SyncManager::LogUnsyncedItems(int level) const {
  std::vector<int64> unsynced_handles;
  sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
  trans.GetWrappedTrans()->directory()->GetUnsyncedMetaHandles(
      trans.GetWrappedTrans(), &unsynced_handles);

  for (std::vector<int64>::const_iterator it = unsynced_handles.begin();
       it != unsynced_handles.end(); ++it) {
    ReadNode node(&trans);
    if (node.InitByIdLookup(*it)) {
      scoped_ptr<DictionaryValue> value(node.GetDetailsAsValue());
      std::string info;
      base::JSONWriter::Write(value.get(), true, &info);
      VLOG(level) << info;
    }
  }
}

void SyncManager::TriggerOnNotificationStateChangeForTest(
    bool notifications_enabled) {
  data_->OnNotificationStateChange(notifications_enabled);
}

void SyncManager::TriggerOnIncomingNotificationForTest(
    const syncable::ModelTypeBitSet& model_types) {
  syncable::ModelTypePayloadMap model_types_with_payloads =
      syncable::ModelTypePayloadMapFromBitSet(model_types,
          std::string());

  data_->OnIncomingNotification(model_types_with_payloads);
}

}  // namespace sync_api
