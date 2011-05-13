// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_model_associator.h"

#include <set>

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "net/base/escape.h"
#include "webkit/glue/password_form.h"

namespace browser_sync {

const char kPasswordTag[] = "google_chrome_passwords";

PasswordModelAssociator::PasswordModelAssociator(
    ProfileSyncService* sync_service,
    PasswordStore* password_store)
    : sync_service_(sync_service),
      password_store_(password_store),
      password_node_id_(sync_api::kInvalidId),
      abort_association_pending_(false),
      expected_loop_(MessageLoop::current()) {
  DCHECK(sync_service_);
  DCHECK(password_store_);
#if defined(OS_MACOSX)
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
#else
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
#endif
}

PasswordModelAssociator::~PasswordModelAssociator() {}

bool PasswordModelAssociator::AssociateModels() {
  DCHECK(expected_loop_ == MessageLoop::current());
  {
    base::AutoLock lock(abort_association_pending_lock_);
    abort_association_pending_ = false;
  }

  // We must not be holding a transaction when we interact with the password
  // store, as it can post tasks to the UI thread which can itself be blocked
  // on our transaction, resulting in deadlock. (http://crbug.com/70658)
  std::vector<webkit_glue::PasswordForm*> passwords;
  if (!password_store_->FillAutofillableLogins(&passwords) ||
      !password_store_->FillBlacklistLogins(&passwords)) {
    STLDeleteElements(&passwords);
    LOG(ERROR) << "Could not get the password entries.";
    return false;
  }

  std::set<std::string> current_passwords;
  PasswordVector new_passwords;
  PasswordVector updated_passwords;
  {
    sync_api::WriteTransaction trans(sync_service_->GetUserShare());
    sync_api::ReadNode password_root(&trans);
    if (!password_root.InitByTagLookup(kPasswordTag)) {
      LOG(ERROR) << "Server did not create the top-level password node. We "
                 << "might be running against an out-of-date server.";
      return false;
    }

    for (std::vector<webkit_glue::PasswordForm*>::iterator ix =
             passwords.begin();
         ix != passwords.end(); ++ix) {
      if (IsAbortPending())
        return false;
      std::string tag = MakeTag(**ix);

      sync_api::ReadNode node(&trans);
      if (node.InitByClientTagLookup(syncable::PASSWORDS, tag)) {
        const sync_pb::PasswordSpecificsData& password =
            node.GetPasswordSpecifics();
        DCHECK_EQ(tag, MakeTag(password));

        webkit_glue::PasswordForm new_password;

        if (MergePasswords(password, **ix, &new_password)) {
          sync_api::WriteNode write_node(&trans);
          if (!write_node.InitByClientTagLookup(syncable::PASSWORDS, tag)) {
            STLDeleteElements(&passwords);
            LOG(ERROR) << "Failed to edit password sync node.";
            return false;
          }
          WriteToSyncNode(new_password, &write_node);
          updated_passwords.push_back(new_password);
        }

        Associate(&tag, node.GetId());
      } else {
        sync_api::WriteNode node(&trans);
        if (!node.InitUniqueByCreation(syncable::PASSWORDS,
                                       password_root, tag)) {
          STLDeleteElements(&passwords);
          LOG(ERROR) << "Failed to create password sync node.";
          return false;
        }

        WriteToSyncNode(**ix, &node);

        Associate(&tag, node.GetId());
      }

      current_passwords.insert(tag);
    }

    STLDeleteElements(&passwords);

    int64 sync_child_id = password_root.GetFirstChildId();
    while (sync_child_id != sync_api::kInvalidId) {
      sync_api::ReadNode sync_child_node(&trans);
      if (!sync_child_node.InitByIdLookup(sync_child_id)) {
        LOG(ERROR) << "Failed to fetch child node.";
        return false;
      }
      const sync_pb::PasswordSpecificsData& password =
          sync_child_node.GetPasswordSpecifics();
      std::string tag = MakeTag(password);

      // The password only exists on the server.  Add it to the local
      // model.
      if (current_passwords.find(tag) == current_passwords.end()) {
        webkit_glue::PasswordForm new_password;

        CopyPassword(password, &new_password);
        Associate(&tag, sync_child_node.GetId());
        new_passwords.push_back(new_password);
      }

      sync_child_id = sync_child_node.GetSuccessorId();
    }
  }

  // We must not be holding a transaction when we interact with the password
  // store, as it can post tasks to the UI thread which can itself be blocked
  // on our transaction, resulting in deadlock. (http://crbug.com/70658)
  if (!WriteToPasswordStore(&new_passwords, &updated_passwords, NULL)) {
    LOG(ERROR) << "Failed to write passwords.";
    return false;
  }

  return true;
}

bool PasswordModelAssociator::DeleteAllNodes(
    sync_api::WriteTransaction* trans) {
  DCHECK(expected_loop_ == MessageLoop::current());
  for (PasswordToSyncIdMap::iterator node_id = id_map_.begin();
       node_id != id_map_.end(); ++node_id) {
    sync_api::WriteNode sync_node(trans);
    if (!sync_node.InitByIdLookup(node_id->second)) {
      LOG(ERROR) << "Typed url node lookup failed.";
      return false;
    }
    sync_node.Remove();
  }

  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool PasswordModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool PasswordModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  int64 password_sync_id;
  if (!GetSyncIdForTaggedNode(kPasswordTag, &password_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level password node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());

  sync_api::ReadNode password_node(&trans);
  if (!password_node.InitByIdLookup(password_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level password node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the password folder has any
  // children.
  *has_nodes = sync_api::kInvalidId != password_node.GetFirstChildId();
  return true;
}

void PasswordModelAssociator::AbortAssociation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(abort_association_pending_lock_);
  abort_association_pending_ = true;
}

bool PasswordModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  // We always encrypt passwords, so no need to check if encryption is enabled.
  return sync_service_->IsCryptographerReady(&trans);
}

const std::string* PasswordModelAssociator::GetChromeNodeFromSyncId(
    int64 sync_id) {
  return NULL;
}

bool PasswordModelAssociator::InitSyncNodeFromChromeId(
    const std::string& node_id,
    sync_api::BaseNode* sync_node) {
  return false;
}

bool PasswordModelAssociator::IsAbortPending() {
  base::AutoLock lock(abort_association_pending_lock_);
  return abort_association_pending_;
}

int64 PasswordModelAssociator::GetSyncIdFromChromeId(
    const std::string& password) {
  PasswordToSyncIdMap::const_iterator iter = id_map_.find(password);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void PasswordModelAssociator::Associate(
    const std::string* password, int64 sync_id) {
  DCHECK(expected_loop_ == MessageLoop::current());
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(*password) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[*password] = sync_id;
  id_map_inverse_[sync_id] = *password;
}

void PasswordModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(expected_loop_ == MessageLoop::current());
  SyncIdToPasswordMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  CHECK(id_map_.erase(iter->second));
  id_map_inverse_.erase(iter);
}

bool PasswordModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                     int64* sync_id) {
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

bool PasswordModelAssociator::WriteToPasswordStore(
         const PasswordVector* new_passwords,
         const PasswordVector* updated_passwords,
         const PasswordVector* deleted_passwords) {
  if (new_passwords) {
    for (PasswordVector::const_iterator password = new_passwords->begin();
         password != new_passwords->end(); ++password) {
      password_store_->AddLoginImpl(*password);
    }
  }

  if (updated_passwords) {
    for (PasswordVector::const_iterator password = updated_passwords->begin();
         password != updated_passwords->end(); ++password) {
      password_store_->UpdateLoginImpl(*password);
    }
  }

  if (deleted_passwords) {
    for (PasswordVector::const_iterator password = deleted_passwords->begin();
         password != deleted_passwords->end(); ++password) {
      password_store_->RemoveLoginImpl(*password);
    }
  }

  if (new_passwords || updated_passwords || deleted_passwords) {
    // We have to notify password store observers of the change by hand since
    // we use internal password store interfaces to make changes synchronously.
    password_store_->PostNotifyLoginsChanged();
  }
  return true;
}

// static
void PasswordModelAssociator::CopyPassword(
        const sync_pb::PasswordSpecificsData& password,
        webkit_glue::PasswordForm* new_password) {
  new_password->scheme =
      static_cast<webkit_glue::PasswordForm::Scheme>(password.scheme());
  new_password->signon_realm = password.signon_realm();
  new_password->origin = GURL(password.origin());
  new_password->action = GURL(password.action());
  new_password->username_element =
      UTF8ToUTF16(password.username_element());
  new_password->password_element =
      UTF8ToUTF16(password.password_element());
  new_password->username_value =
      UTF8ToUTF16(password.username_value());
  new_password->password_value =
      UTF8ToUTF16(password.password_value());
  new_password->ssl_valid = password.ssl_valid();
  new_password->preferred = password.preferred();
  new_password->date_created =
      base::Time::FromInternalValue(password.date_created());
  new_password->blacklisted_by_user =
      password.blacklisted();
}

// static
bool PasswordModelAssociator::MergePasswords(
        const sync_pb::PasswordSpecificsData& password,
        const webkit_glue::PasswordForm& password_form,
        webkit_glue::PasswordForm* new_password) {
  DCHECK(new_password);

  if (password.scheme() == password_form.scheme &&
      password_form.signon_realm == password.signon_realm() &&
      password_form.origin.spec() == password.origin() &&
      password_form.action.spec() == password.action() &&
      UTF16ToUTF8(password_form.username_element) ==
          password.username_element() &&
      UTF16ToUTF8(password_form.password_element) ==
          password.password_element() &&
      UTF16ToUTF8(password_form.username_value) ==
          password.username_value() &&
      UTF16ToUTF8(password_form.password_value) ==
          password.password_value() &&
      password.ssl_valid() == password_form.ssl_valid &&
      password.preferred() == password_form.preferred &&
      password.date_created() == password_form.date_created.ToInternalValue() &&
      password.blacklisted() == password_form.blacklisted_by_user) {
    return false;
  }

  // If the passwords differ, we take the one that was created more recently.
  if (base::Time::FromInternalValue(password.date_created()) <=
      password_form.date_created) {
    *new_password = password_form;
  } else {
    CopyPassword(password, new_password);
  }

  return true;
}

// static
void PasswordModelAssociator::WriteToSyncNode(
         const webkit_glue::PasswordForm& password_form,
         sync_api::WriteNode* node) {
  sync_pb::PasswordSpecificsData password;
  password.set_scheme(password_form.scheme);
  password.set_signon_realm(password_form.signon_realm);
  password.set_origin(password_form.origin.spec());
  password.set_action(password_form.action.spec());
  password.set_username_element(UTF16ToUTF8(password_form.username_element));
  password.set_password_element(UTF16ToUTF8(password_form.password_element));
  password.set_username_value(UTF16ToUTF8(password_form.username_value));
  password.set_password_value(UTF16ToUTF8(password_form.password_value));
  password.set_ssl_valid(password_form.ssl_valid);
  password.set_preferred(password_form.preferred);
  password.set_date_created(password_form.date_created.ToInternalValue());
  password.set_blacklisted(password_form.blacklisted_by_user);

  node->SetPasswordSpecifics(password);
}

// static
std::string PasswordModelAssociator::MakeTag(
                const webkit_glue::PasswordForm& password) {
  return MakeTag(password.origin.spec(),
                 UTF16ToUTF8(password.username_element),
                 UTF16ToUTF8(password.username_value),
                 UTF16ToUTF8(password.password_element),
                 password.signon_realm);
}

// static
std::string PasswordModelAssociator::MakeTag(
                const sync_pb::PasswordSpecificsData& password) {
  return MakeTag(password.origin(),
                 password.username_element(),
                 password.username_value(),
                 password.password_element(),
                 password.signon_realm());
}

// static
std::string PasswordModelAssociator::MakeTag(
    const std::string& origin_url,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm) {
  return EscapePath(origin_url) + "|" +
         EscapePath(username_element) + "|" +
         EscapePath(username_value) + "|" +
         EscapePath(password_element) + "|" +
         EscapePath(signon_realm);
}

}  // namespace browser_sync
