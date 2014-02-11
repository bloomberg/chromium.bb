// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_change_processor.h"

#include <string>

#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/password_specifics.pb.h"

using content::BrowserThread;

namespace browser_sync {

PasswordChangeProcessor::PasswordChangeProcessor(
    PasswordModelAssociator* model_associator,
    PasswordStore* password_store,
    DataTypeErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      model_associator_(model_associator),
      password_store_(password_store),
      expected_loop_(base::MessageLoop::current()),
      disconnected_(false) {
  DCHECK(model_associator);
  DCHECK(error_handler);
  DCHECK(password_store->GetBackgroundTaskRunner()->RunsTasksOnCurrentThread());
}

PasswordChangeProcessor::~PasswordChangeProcessor() {
  base::AutoLock lock(disconnect_lock_);
  password_store_->RemoveObserver(this);
}

void PasswordChangeProcessor::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK(expected_loop_ == base::MessageLoop::current());

  base::AutoLock lock(disconnect_lock_);
  if (disconnected_)
    return;

  syncer::WriteTransaction trans(FROM_HERE, share_handle());

  syncer::ReadNode password_root(&trans);
  if (password_root.InitByTagLookup(kPasswordTag) !=
          syncer::BaseNode::INIT_OK) {
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "Server did not create the top-level password node. "
        "We might be running against an out-of-date server.");
    return;
  }

  for (PasswordStoreChangeList::const_iterator change = changes.begin();
       change != changes.end(); ++change) {
    std::string tag = PasswordModelAssociator::MakeTag(change->form());
    switch (change->type()) {
      case PasswordStoreChange::ADD: {
        syncer::WriteNode sync_node(&trans);
        syncer::WriteNode::InitUniqueByCreationResult result =
            sync_node.InitUniqueByCreation(syncer::PASSWORDS, password_root,
                                           tag);
        if (result == syncer::WriteNode::INIT_SUCCESS) {
          PasswordModelAssociator::WriteToSyncNode(change->form(), &sync_node);
          model_associator_->Associate(&tag, sync_node.GetId());
          break;
        } else {
          // Maybe this node already exists and we should update it.
          //
          // If the PasswordStore is told to add an entry but an entry with the
          // same name already exists, it will overwrite it.  It will report
          // this change as an ADD rather than an UPDATE.  Ideally, it would be
          // able to tell us what action was actually taken, rather than what
          // action was requested.  If it did so, we wouldn't need to fall back
          // to trying to update an existing password node here.
          //
          // TODO: Remove this.  See crbug.com/87855.
          int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
          if (syncer::kInvalidId == sync_id) {
            error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                "Unable to create or retrieve password node");
            LOG(ERROR) << "Invalid sync id.";
            return;
          }
          if (sync_node.InitByIdLookup(sync_id) !=
                  syncer::BaseNode::INIT_OK) {
            error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                "Password node lookup failed.");
            LOG(ERROR) << "Password node lookup failed.";
            return;
          }
          PasswordModelAssociator::WriteToSyncNode(change->form(), &sync_node);
          break;
        }
      }
      case PasswordStoreChange::UPDATE: {
        syncer::WriteNode sync_node(&trans);
        int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
        if (syncer::kInvalidId == sync_id) {
          error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
              "Invalid sync id");
          LOG(ERROR) << "Invalid sync id.";
          return;
        } else {
          if (sync_node.InitByIdLookup(sync_id) !=
                  syncer::BaseNode::INIT_OK) {
            error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                "Password node lookup failed.");
            LOG(ERROR) << "Password node lookup failed.";
            return;
          }
        }

        PasswordModelAssociator::WriteToSyncNode(change->form(), &sync_node);
        break;
      }
      case PasswordStoreChange::REMOVE: {
        syncer::WriteNode sync_node(&trans);
        int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
        if (syncer::kInvalidId == sync_id) {
          // We've been asked to remove a password that we don't know about.
          // That's weird, but apparently we were already in the requested
          // state, so it's not really an unrecoverable error. Just return.
          LOG(WARNING) << "Trying to delete nonexistent password sync node!";
          return;
        } else {
          if (sync_node.InitByIdLookup(sync_id) !=
                  syncer::BaseNode::INIT_OK) {
            error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                "Password node lookup failed.");
            return;
          }
          model_associator_->Disassociate(sync_node.GetId());
          sync_node.Tombstone();
        }
        break;
      }
    }
  }
}

void PasswordChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    int64 model_version,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  base::AutoLock lock(disconnect_lock_);
  if (disconnected_)
    return;

  syncer::ReadNode password_root(trans);
  if (password_root.InitByTagLookup(kPasswordTag) !=
          syncer::BaseNode::INIT_OK) {
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "Password root node lookup failed.");
    return;
  }

  DCHECK(deleted_passwords_.empty() && new_passwords_.empty() &&
         updated_passwords_.empty());

  for (syncer::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    if (syncer::ChangeRecord::ACTION_DELETE ==
        it->action) {
      DCHECK(it->specifics.has_password())
          << "Password specifics data not present on delete!";
      DCHECK(it->extra.get());
      syncer::ExtraPasswordChangeRecordData* extra =
          it->extra.get();
      const sync_pb::PasswordSpecificsData& password = extra->unencrypted();
      autofill::PasswordForm form;
      PasswordModelAssociator::CopyPassword(password, &form);
      deleted_passwords_.push_back(form);
      model_associator_->Disassociate(it->id);
      continue;
    }

    syncer::ReadNode sync_node(trans);
    if (sync_node.InitByIdLookup(it->id) != syncer::BaseNode::INIT_OK) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Password node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the passwords folder.
    DCHECK_EQ(password_root.GetId(), sync_node.GetParentId());
    DCHECK_EQ(syncer::PASSWORDS, sync_node.GetModelType());

    const sync_pb::PasswordSpecificsData& password_data =
        sync_node.GetPasswordSpecifics();
    autofill::PasswordForm password;
    PasswordModelAssociator::CopyPassword(password_data, &password);

    if (syncer::ChangeRecord::ACTION_ADD == it->action) {
      std::string tag(PasswordModelAssociator::MakeTag(password));
      model_associator_->Associate(&tag, sync_node.GetId());
      new_passwords_.push_back(password);
    } else {
      DCHECK_EQ(syncer::ChangeRecord::ACTION_UPDATE, it->action);
      updated_passwords_.push_back(password);
    }
  }
}

void PasswordChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  base::AutoLock lock(disconnect_lock_);
  if (disconnected_)
    return;

  ScopedStopObserving<PasswordChangeProcessor> stop_observing(this);

  syncer::SyncError error = model_associator_->WriteToPasswordStore(
      &new_passwords_,
      &updated_passwords_,
      &deleted_passwords_);
  if (error.IsSet()) {
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "Error writing passwords");
    return;
  }

  deleted_passwords_.clear();
  new_passwords_.clear();
  updated_passwords_.clear();
}

void PasswordChangeProcessor::Disconnect() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(disconnect_lock_);
  disconnected_ = true;
}

void PasswordChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  password_store_->ScheduleTask(
      base::Bind(&PasswordChangeProcessor::InitObserving,
                 base::Unretained(this)));
}

void PasswordChangeProcessor::InitObserving() {
  base::AutoLock lock(disconnect_lock_);
  if (disconnected_)
    return;
  StartObserving();
}

void PasswordChangeProcessor::StartObserving() {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  disconnect_lock_.AssertAcquired();
  password_store_->AddObserver(this);
}

void PasswordChangeProcessor::StopObserving() {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  disconnect_lock_.AssertAcquired();
  password_store_->RemoveObserver(this);
}

}  // namespace browser_sync
