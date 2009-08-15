// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION
#include "chrome/browser/sync/profile_sync_service.h"

#include <stack>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/personalization.h"
#include "chrome/browser/sync/personalization_strings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/time_format.h"
#include "views/window/window.h"

using browser_sync::ModelAssociator;
using browser_sync::SyncBackendHost;

// Default sync server URL.
static const char kSyncServerUrl[] = "https://clients4.google.com/chrome-sync";

ProfileSyncService::ProfileSyncService(Profile* profile)
    : last_auth_error_(AUTH_ERROR_NONE),
      profile_(profile),
      sync_service_url_(kSyncServerUrl),
      backend_initialized_(false),
      expecting_first_run_auth_needed_event_(false),
      is_auth_in_progress_(false),
      ready_to_process_changes_(false),
      unrecoverable_error_detected_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(wizard_(this)) {
}

ProfileSyncService::~ProfileSyncService() {
  Shutdown(false);
}

void ProfileSyncService::Initialize() {
  InitSettings();
  RegisterPreferences();
  if (!profile()->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted))
    DisableForUser();  // Clean up in case of previous crash / setup abort.
  else
    StartUp();
}

void ProfileSyncService::InitSettings() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Override the sync server URL from the command-line, if sync server and sync
  // port command-line arguments exist.
  if (command_line.HasSwitch(switches::kSyncServiceURL)) {
    std::wstring value(command_line.GetSwitchValue(switches::kSyncServiceURL));
    if (!value.empty()) {
      GURL custom_sync_url(WideToUTF8(value));
      if (custom_sync_url.is_valid()) {
        sync_service_url_ = custom_sync_url;
      } else {
        LOG(WARNING) << "The following sync URL specified at the command-line "
                     << "is invalid: " << value;
      }
    }
  }
}

void ProfileSyncService::RegisterPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service->IsPrefRegistered(prefs::kSyncLastSyncedTime))
    return;
  pref_service->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  pref_service->RegisterBooleanPref(prefs::kSyncHasSetupCompleted, false);
}

void ProfileSyncService::ClearPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service->ClearPref(prefs::kSyncHasSetupCompleted);

  pref_service->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::InitializeBackend() {
  backend_->Initialize(sync_service_url_);
}

void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get())
    return;

  last_synced_time_ = base::Time::FromInternalValue(
      profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));

  backend_.reset(new SyncBackendHost(this, profile_->GetPath()));

  // We add ourselves as an observer, and we remain one forever. Note we don't
  // keep any pointer to the model, we just receive notifications from it.
  BookmarkModel* model = profile_->GetBookmarkModel();
  model->AddObserver(this);

  // Create new model assocation manager.
  model_associator_ = new ModelAssociator(this);

  // TODO(timsteele): HttpBridgeFactory should take a const* to the profile's
  // URLRequestContext, because it needs it to create HttpBridge objects, and
  // it may need to do that before the default request context has been set
  // up. For now, call GetRequestContext lazy-init to force creation.
  profile_->GetRequestContext();
  InitializeBackend();
}

void ProfileSyncService::Shutdown(bool sync_disabled) {
  if (backend_.get()) {
    backend_->Shutdown(sync_disabled);
    backend_.reset();
  }

  BookmarkModel* model = profile_->GetBookmarkModel();
  if (model)
    model->RemoveObserver(this);

  // Clear all assocations and throw away the assocation manager instance.
  if (model_associator_.get()) {
    model_associator_->ClearAll();
    model_associator_ = NULL;
  }

  // Clear various flags.
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  expecting_first_run_auth_needed_event_ = false;
  ready_to_process_changes_ = false;
  last_attempted_user_email_.clear();
}

void ProfileSyncService::EnableForUser() {
  if (wizard_.IsVisible()) {
    // TODO(timsteele): Focus wizard.
    return;
  }
  expecting_first_run_auth_needed_event_ = true;

  StartUp();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::DisableForUser() {
  if (wizard_.IsVisible()) {
    // TODO(timsteele): Focus wizard.
    return;
  }
  Shutdown(true);
  ClearPreferences();

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::Loaded(BookmarkModel* model) {
  StartProcessingChangesIfReady();
}

void ProfileSyncService::UpdateSyncNodeProperties(const BookmarkNode* src,
                                                  sync_api::WriteNode* dst) {
  // Set the properties of the item.
  dst->SetIsFolder(src->is_folder());
  dst->SetTitle(WideToUTF16(src->GetTitle()).c_str());
  // URL is passed as a C string here because this interface avoids
  // string16. SetURL copies the data into its own memory.
  string16 url = UTF8ToUTF16(src->GetURL().spec());
  dst->SetURL(url.c_str());
  SetSyncNodeFavicon(src, dst);
}

void ProfileSyncService::EncodeFavicon(const BookmarkNode* src,
                                       std::vector<unsigned char>* dst) const {
  const SkBitmap& favicon = profile_->GetBookmarkModel()->GetFavIcon(src);

  dst->clear();

  // Check for zero-dimension images.  This can happen if the favicon is
  // still being loaded.
  if (favicon.empty())
    return;

  // Re-encode the BookmarkNode's favicon as a PNG, and pass the data to the
  // sync subsystem.
  if (!PNGEncoder::EncodeBGRASkBitmap(favicon, false, dst))
    return;
}

void ProfileSyncService::RemoveOneSyncNode(sync_api::WriteTransaction* trans,
                                           const BookmarkNode* node) {
  sync_api::WriteNode sync_node(trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(node->id(), &sync_node)) {
    SetUnrecoverableError();
    return;
  }
  // This node should have no children.
  DCHECK(sync_node.GetFirstChildId() == sync_api::kInvalidId);
  // Remove association and delete the sync node.
  model_associator_->DisassociateIds(sync_node.GetId());
  sync_node.Remove();
}

void ProfileSyncService::RemoveSyncNodeHierarchy(const BookmarkNode* topmost) {
  sync_api::WriteTransaction trans(backend_->GetUserShareHandle());

  // Later logic assumes that |topmost| has been unlinked.
  DCHECK(!topmost->GetParent());

  // A BookmarkModel deletion event means that |node| and all its children were
  // deleted. Sync backend expects children to be deleted individually, so we do
  // a depth-first-search here.  At each step, we consider the |index|-th child
  // of |node|.  |index_stack| stores index values for the parent levels.
  std::stack<int> index_stack;
  index_stack.push(0);  // For the final pop.  It's never used.
  const BookmarkNode* node = topmost;
  int index = 0;
  while (node) {
    // The top of |index_stack| should always be |node|'s index.
    DCHECK(!node->GetParent() || (node->GetParent()->IndexOfChild(node) ==
      index_stack.top()));
    if (index == node->GetChildCount()) {
      // If we've processed all of |node|'s children, delete |node| and move
      // on to its successor.
      RemoveOneSyncNode(&trans, node);
      node = node->GetParent();
      index = index_stack.top() + 1;      // (top() + 0) was what we removed.
      index_stack.pop();
    } else {
      // If |node| has an unprocessed child, process it next after pushing the
      // current state onto the stack.
      DCHECK_LT(index, node->GetChildCount());
      index_stack.push(index);
      node = node->GetChild(index);
      index = 0;
    }
  }
  DCHECK(index_stack.empty());  // Nothing should be left on the stack.
}

bool ProfileSyncService::MergeAndSyncAcceptanceNeeded() const {
  // If we've shown the dialog before, don't show it again.
  if (profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted))
    return false;

  return model_associator_->BookmarkModelHasUserCreatedNodes() &&
         model_associator_->SyncModelHasUserCreatedNodes();
}

bool ProfileSyncService::HasSyncSetupCompleted() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void ProfileSyncService::SetSyncSetupCompleted() {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  prefs->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  last_synced_time_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kSyncLastSyncedTime,
      last_synced_time_.ToInternalValue());
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index) {
  if (!ShouldPushChanges())
    return;

  DCHECK(backend_->GetUserShareHandle());

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(backend_->GetUserShareHandle());

  CreateSyncNode(parent, index, &trans);
}

int64 ProfileSyncService::CreateSyncNode(const BookmarkNode* parent,
                                         int index,
                                         sync_api::WriteTransaction* trans) {
  const BookmarkNode* child = parent->GetChild(index);
  DCHECK(child);

  // Create a WriteNode container to hold the new node.
  sync_api::WriteNode sync_child(trans);

  // Actually create the node with the appropriate initial position.
  if (!PlaceSyncNode(CREATE, parent, index, trans, &sync_child)) {
    LOG(WARNING) << "Sync node creation failed; recovery unlikely";
    SetUnrecoverableError();
    return sync_api::kInvalidId;
  }

  UpdateSyncNodeProperties(child, &sync_child);

  // Associate the ID from the sync domain with the bookmark node, so that we
  // can refer back to this item later.
  model_associator_->AssociateIds(child->id(), sync_child.GetId());

  return sync_child.GetId();
}

void ProfileSyncService::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int index,
                                             const BookmarkNode* node) {
  if (!ShouldPushChanges())
    return;

  RemoveSyncNodeHierarchy(node);
}

void ProfileSyncService::BookmarkNodeChanged(BookmarkModel* model,
                                             const BookmarkNode* node) {
  if (!ShouldPushChanges())
    return;

  // We shouldn't see changes to the top-level nodes.
  DCHECK_NE(node, model->GetBookmarkBarNode());
  DCHECK_NE(node, model->other_node());

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(backend_->GetUserShareHandle());

  // Lookup the sync node that's associated with |node|.
  sync_api::WriteNode sync_node(&trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(node->id(), &sync_node)) {
    SetUnrecoverableError();
    return;
  }

  UpdateSyncNodeProperties(node, &sync_node);

  DCHECK_EQ(sync_node.GetIsFolder(), node->is_folder());
  DCHECK_EQ(model_associator_->GetBookmarkNodeFromSyncId(
            sync_node.GetParentId()),
            node->GetParent());
  // This node's index should be one more than the predecessor's index.
  DCHECK_EQ(node->GetParent()->IndexOfChild(node),
            CalculateBookmarkModelInsertionIndex(node->GetParent(),
                                                 &sync_node));
}

void ProfileSyncService::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           int old_index,
                                           const BookmarkNode* new_parent,
                                           int new_index) {
  if (!ShouldPushChanges())
    return;

  const BookmarkNode* child = new_parent->GetChild(new_index);
  // We shouldn't see changes to the top-level nodes.
  DCHECK_NE(child, model->GetBookmarkBarNode());
  DCHECK_NE(child, model->other_node());

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(backend_->GetUserShareHandle());

  // Lookup the sync node that's associated with |child|.
  sync_api::WriteNode sync_node(&trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(child->id(),
                                                     &sync_node)) {
    SetUnrecoverableError();
    return;
  }

  if (!PlaceSyncNode(MOVE, new_parent, new_index, &trans, &sync_node)) {
    SetUnrecoverableError();
    return;
  }
}

void ProfileSyncService::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  BookmarkNodeChanged(model, node);
}

void ProfileSyncService::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  if (!ShouldPushChanges())
    return;

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(backend_->GetUserShareHandle());

  // The given node's children got reordered. We need to reorder all the
  // children of the corresponding sync node.
  for (int i = 0; i < node->GetChildCount(); ++i) {
    sync_api::WriteNode sync_child(&trans);
    if (!model_associator_->InitSyncNodeFromBookmarkId(node->GetChild(i)->id(),
                                                       &sync_child)) {
      SetUnrecoverableError();
      return;
    }
    DCHECK_EQ(sync_child.GetParentId(),
              model_associator_->GetSyncIdFromBookmarkId(node->id()));

    if (!PlaceSyncNode(MOVE, node, i, &trans, &sync_child)) {
      SetUnrecoverableError();
      return;
    }
  }
}

bool ProfileSyncService::PlaceSyncNode(MoveOrCreate operation,
                                       const BookmarkNode* parent,
                                       int index,
                                       sync_api::WriteTransaction* trans,
                                       sync_api::WriteNode* dst) {
  sync_api::ReadNode sync_parent(trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(parent->id(),
                                                     &sync_parent)) {
    LOG(WARNING) << "Parent lookup failed";
    SetUnrecoverableError();
    return false;
  }

  bool success = false;
  if (index == 0) {
    // Insert into first position.
    success = (operation == CREATE) ? dst->InitByCreation(sync_parent, NULL) :
                                      dst->SetPosition(sync_parent, NULL);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetId(), sync_parent.GetFirstChildId());
      DCHECK_EQ(dst->GetPredecessorId(), sync_api::kInvalidId);
    }
  } else {
    // Find the bookmark model predecessor, and insert after it.
    const BookmarkNode* prev = parent->GetChild(index - 1);
    sync_api::ReadNode sync_prev(trans);
    if (!model_associator_->InitSyncNodeFromBookmarkId(prev->id(),
                                                       &sync_prev)) {
      LOG(WARNING) << "Predecessor lookup failed";
      return false;
    }
    success = (operation == CREATE) ?
        dst->InitByCreation(sync_parent, &sync_prev) :
        dst->SetPosition(sync_parent, &sync_prev);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetPredecessorId(), sync_prev.GetId());
      DCHECK_EQ(dst->GetId(), sync_prev.GetSuccessorId());
    }
  }
  return success;
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::SetUnrecoverableError() {
  unrecoverable_error_detected_ = true;
  LOG(ERROR) << "Unrecoverable error detected -- ProfileSyncService unusable.";
}

// Determine the bookmark model index to which a node must be moved so that
// predecessor of the node (in the bookmark model) matches the predecessor of
// |source| (in the sync model).
// As a precondition, this assumes that the predecessor of |source| has been
// updated and is already in the correct position in the bookmark model.
int ProfileSyncService::CalculateBookmarkModelInsertionIndex(
    const BookmarkNode* parent,
    const sync_api::BaseNode* child_info) const {
  DCHECK(parent);
  DCHECK(child_info);
  int64 predecessor_id = child_info->GetPredecessorId();
  // A return ID of kInvalidId indicates no predecessor.
  if (predecessor_id == sync_api::kInvalidId)
    return 0;

  // Otherwise, insert after the predecessor bookmark node.
  const BookmarkNode* predecessor =
      model_associator_->GetBookmarkNodeFromSyncId(predecessor_id);
  DCHECK(predecessor);
  DCHECK_EQ(predecessor->GetParent(), parent);
  return parent->IndexOfChild(predecessor) + 1;
}

void ProfileSyncService::OnBackendInitialized() {
  backend_initialized_ = true;
  StartProcessingChangesIfReady();

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null())
    UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnAuthError() {
  last_auth_error_ = backend_->GetAuthErrorState();
  // Protect against the in-your-face dialogs that pop out of nowhere.
  // Require the user to click somewhere to run the setup wizard in the case
  // of a steady-state auth failure.
  if (wizard_.IsVisible() || expecting_first_run_auth_needed_event_) {
    wizard_.Step(AUTH_ERROR_NONE == backend_->GetAuthErrorState() ?
        SyncSetupWizard::GAIA_SUCCESS : SyncSetupWizard::GAIA_LOGIN);
  }

  if (expecting_first_run_auth_needed_event_) {
    last_auth_error_ = AUTH_ERROR_NONE;
    expecting_first_run_auth_needed_event_ = false;
  }

  if (!wizard_.IsVisible()) {
    auth_error_time_ == base::TimeTicks::Now();
  }

  is_auth_in_progress_ = false;
  // Fan the notification out to interested UI-thread components.
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::ShowLoginDialog() {
  if (wizard_.IsVisible())
    return;

  if (!auth_error_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.ReauthorizationTime",
                             base::TimeTicks::Now() - auth_error_time_);
    auth_error_time_ = base::TimeTicks();  // Reset auth_error_time_ to null.
  }

  if (last_auth_error_ != AUTH_ERROR_NONE)
    wizard_.Step(SyncSetupWizard::GAIA_LOGIN);
}

// ApplyModelChanges is called by the sync backend after changes have been made
// to the sync engine's model.  Apply these changes to the browser bookmark
// model.
void ProfileSyncService::ApplyModelChanges(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!ShouldPushChanges())
    return;

  // A note about ordering.  Sync backend is responsible for ordering the change
  // records in the following order:
  //
  // 1. Deletions, from leaves up to parents.
  // 2. Existing items with synced parents & predecessors.
  // 3. New items with synced parents & predecessors.
  // 4. Items with parents & predecessors in the list.
  // 5. Repeat #4 until all items are in the list.
  //
  // "Predecessor" here means the previous item within a given folder; an item
  // in the first position is always said to have a synced predecessor.
  // For the most part, applying these changes in the order given will yield
  // the correct result.  There is one exception, however: for items that are
  // moved away from a folder that is being deleted, we will process the delete
  // before the move.  Since deletions in the bookmark model propagate from
  // parent to child, we must move them to a temporary location.
  BookmarkModel* model = profile_->GetBookmarkModel();

  // We are going to make changes to the bookmarks model, but don't want to end
  // up in a feedback loop, so remove ourselves as an observer while applying
  // changes.
  model->RemoveObserver(this);

  // A parent to hold nodes temporarily orphaned by parent deletion.  It is
  // lazily created inside the loop.
  const BookmarkNode* foster_parent = NULL;
  for (int i = 0; i < change_count; ++i) {
    const BookmarkNode* dst =
        model_associator_->GetBookmarkNodeFromSyncId(changes[i].id);
    // Ignore changes to the permanent top-level nodes.  We only care about
    // their children.
    if ((dst == model->GetBookmarkBarNode()) || (dst == model->other_node()))
      continue;
    if (changes[i].action ==
        sync_api::SyncManager::ChangeRecord::ACTION_DELETE) {
      // Deletions should always be at the front of the list.
      DCHECK(i == 0 || changes[i-1].action == changes[i].action);
      // Children of a deleted node should not be deleted; they may be
      // reparented by a later change record.  Move them to a temporary place.
      DCHECK(dst) << "Could not find node to be deleted";
      const BookmarkNode* parent = dst->GetParent();
      if (dst->GetChildCount()) {
        if (!foster_parent) {
          foster_parent = model->AddGroup(model->other_node(),
                                          model->other_node()->GetChildCount(),
                                          std::wstring());
        }
        for (int i = dst->GetChildCount() - 1; i >= 0; --i) {
          model->Move(dst->GetChild(i), foster_parent,
                      foster_parent->GetChildCount());
        }
      }
      DCHECK_EQ(dst->GetChildCount(), 0) << "Node being deleted has children";
      model->Remove(parent, parent->IndexOfChild(dst));
      dst = NULL;
      model_associator_->DisassociateIds(changes[i].id);
    } else {
      DCHECK_EQ((changes[i].action ==
          sync_api::SyncManager::ChangeRecord::ACTION_ADD), (dst == NULL))
          << "ACTION_ADD should be seen if and only if the node is unknown.";

      sync_api::ReadNode src(trans);
      if (!src.InitByIdLookup(changes[i].id)) {
        LOG(ERROR) << "ApplyModelChanges was passed a bad ID";
        SetUnrecoverableError();
        return;
      }

      CreateOrUpdateBookmarkNode(&src, model);
    }
  }
  // Clean up the temporary node.
  if (foster_parent) {
    // There should be no nodes left under the foster parent.
    DCHECK_EQ(foster_parent->GetChildCount(), 0);
    model->Remove(foster_parent->GetParent(),
                  foster_parent->GetParent()->IndexOfChild(foster_parent));
    foster_parent = NULL;
  }

  // We are now ready to hear about bookmarks changes again.
  model->AddObserver(this);
}

// Create a bookmark node corresponding to |src| if one is not already
// associated with |src|.
const BookmarkNode* ProfileSyncService::CreateOrUpdateBookmarkNode(
    sync_api::BaseNode* src,
    BookmarkModel* model) {
  const BookmarkNode* parent =
      model_associator_->GetBookmarkNodeFromSyncId(src->GetParentId());
  if (!parent) {
    DLOG(WARNING) << "Could not find parent of node being added/updated."
      << " Node title: " << src->GetTitle()
      << ", parent id = " << src->GetParentId();
    return NULL;
  }
  int index = CalculateBookmarkModelInsertionIndex(parent, src);
  const BookmarkNode* dst = model_associator_->GetBookmarkNodeFromSyncId(
      src->GetId());
  if (!dst) {
    dst = CreateBookmarkNode(src, parent, index);
    model_associator_->AssociateIds(dst->id(), src->GetId());
  } else {
    // URL and is_folder are not expected to change.
    // TODO(ncarter): Determine if such changes should be legal or not.
    DCHECK_EQ(src->GetIsFolder(), dst->is_folder());

    // Handle reparenting and/or repositioning.
    model->Move(dst, parent, index);

    // Handle title update and URL changes due to possible conflict resolution
    // that can happen if both a local user change and server change occur
    // within a sufficiently small time interval.
    const BookmarkNode* old_dst = dst;
    dst = bookmark_utils::ApplyEditsWithNoGroupChange(model, parent, dst,
        UTF16ToWide(src->GetTitle()),
        src->GetIsFolder() ? GURL() : GURL(src->GetURL()),
        NULL);  // NULL because we don't need a BookmarkEditor::Handler.
    if (dst != old_dst) {  // dst was replaced with a new node with new URL.
      model_associator_->DisassociateIds(src->GetId());
      model_associator_->AssociateIds(dst->id(), src->GetId());
    }
    SetBookmarkFavicon(src, dst);
  }

  return dst;
}

// Creates a bookmark node under the given parent node from the given sync
// node. Returns the newly created node.
const BookmarkNode* ProfileSyncService::CreateBookmarkNode(
    sync_api::BaseNode* sync_node,
    const BookmarkNode* parent,
    int index) const {
  DCHECK(parent);
  DCHECK(index >= 0 && index <= parent->GetChildCount());
  BookmarkModel* model = profile_->GetBookmarkModel();

  const BookmarkNode* node;
  if (sync_node->GetIsFolder()) {
    node = model->AddGroup(parent, index, UTF16ToWide(sync_node->GetTitle()));
  } else {
    GURL url(sync_node->GetURL());
    node = model->AddURL(parent, index,
                         UTF16ToWide(sync_node->GetTitle()), url);
    SetBookmarkFavicon(sync_node, node);
  }
  return node;
}

// Sets the favicon of the given bookmark node from the given sync node.
bool ProfileSyncService::SetBookmarkFavicon(
    sync_api::BaseNode* sync_node,
    const BookmarkNode* bookmark_node) const {
  size_t icon_size = 0;
  const unsigned char* icon_bytes = sync_node->GetFaviconBytes(&icon_size);
  if (!icon_size || !icon_bytes)
    return false;

  // Registering a favicon requires that we provide a source URL, but we
  // don't know where these came from.  Currently we just use the
  // destination URL, which is not correct, but since the favicon URL
  // is used as a key in the history's thumbnail DB, this gives us a value
  // which does not collide with others.
  GURL fake_icon_url = bookmark_node->GetURL();

  std::vector<unsigned char> icon_bytes_vector(icon_bytes,
                                               icon_bytes + icon_size);

  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);

  history->AddPage(bookmark_node->GetURL());
  history->SetFavIcon(bookmark_node->GetURL(),
                      fake_icon_url,
                      icon_bytes_vector);

  return true;
}

void ProfileSyncService::SetSyncNodeFavicon(
    const BookmarkNode* bookmark_node,
    sync_api::WriteNode* sync_node) const {
  std::vector<unsigned char> favicon_bytes;
  EncodeFavicon(bookmark_node, &favicon_bytes);
  if (!favicon_bytes.empty())
    sync_node->SetFaviconBytes(&favicon_bytes[0], favicon_bytes.size());
}

SyncBackendHost::StatusSummary ProfileSyncService::QuerySyncStatusSummary() {
  return backend_->GetStatusSummary();
}

SyncBackendHost::Status ProfileSyncService::QueryDetailedSyncStatus() {
  return backend_->GetDetailedStatus();
}

std::wstring ProfileSyncService::BuildSyncStatusSummaryText(
  const sync_api::SyncManager::Status::Summary& summary) {
  switch (summary) {
    case sync_api::SyncManager::Status::OFFLINE:
      return L"OFFLINE";
    case sync_api::SyncManager::Status::OFFLINE_UNSYNCED:
      return L"OFFLINE_UNSYNCED";
    case sync_api::SyncManager::Status::SYNCING:
      return L"SYNCING";
    case sync_api::SyncManager::Status::READY:
      return L"READY";
    case sync_api::SyncManager::Status::PAUSED:
      return L"PAUSED";
    case sync_api::SyncManager::Status::CONFLICT:
      return L"CONFLICT";
    case sync_api::SyncManager::Status::OFFLINE_UNUSABLE:
      return L"OFFLINE_UNUSABLE";
    case sync_api::SyncManager::Status::INVALID:  // fall through
    default:
      return L"UNKNOWN";
  }
}

std::wstring ProfileSyncService::GetLastSyncedTimeString() const {
  if (last_synced_time_.is_null())
    return kLastSyncedTimeNever;

  base::TimeDelta last_synced = base::Time::Now() - last_synced_time_;

  if (last_synced < base::TimeDelta::FromMinutes(1))
    return kLastSyncedTimeWithinLastMinute;

  return TimeFormat::TimeElapsed(last_synced);
}

string16 ProfileSyncService::GetAuthenticatedUsername() const {
  return backend_->GetAuthenticatedUsername();
}

void ProfileSyncService::OnUserSubmittedAuth(
    const std::string& username, const std::string& password) {
  last_attempted_user_email_ = username;
  is_auth_in_progress_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());

  base::TimeTicks start_time = base::TimeTicks::Now();
  backend_->Authenticate(username, password);
  UMA_HISTOGRAM_TIMES("Sync.AuthorizationTime",
                      base::TimeTicks::Now() - start_time);
}

void ProfileSyncService::OnUserAcceptedMergeAndSync() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.BookmarkAssociationWithUITime",
                      base::TimeTicks::Now() - start_time);

  wizard_.Step(SyncSetupWizard::DONE);  // TODO(timsteele): error state?
  if (!merge_success) {
    LOG(ERROR) << "Model assocation failed.";
    SetUnrecoverableError();
    return;
  }

  ready_to_process_changes_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnUserCancelledDialog() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted)) {
    // A sync dialog was aborted before authentication or merge acceptance.
    // Rollback.
    DisableForUser();
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::StartProcessingChangesIfReady() {
  BookmarkModel* model = profile_->GetBookmarkModel();

  DCHECK(!ready_to_process_changes_);

  // First check if the subsystems are ready.  We can't proceed until they
  // both have finished loading.
  if (!model->IsLoaded())
    return;
  if (!backend_initialized_)
    return;

  // Show the sync merge warning dialog if needed.
  if (MergeAndSyncAcceptanceNeeded()) {
    ProfileSyncService::SyncEvent(MERGE_AND_SYNC_NEEDED);
    wizard_.Step(SyncSetupWizard::MERGE_AND_SYNC);
    return;
  }

  // We're ready to merge the models.
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.BookmarkAssociationTime",
                      base::TimeTicks::Now() - start_time);

  wizard_.Step(SyncSetupWizard::DONE);  // TODO(timsteele): error state?
  if (!merge_success) {
    LOG(ERROR) << "Model assocation failed.";
    SetUnrecoverableError();
    return;
  }

  ready_to_process_changes_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  static LinearHistogram histogram("Sync.EventCodes", MIN_SYNC_EVENT_CODE,
                                   MAX_SYNC_EVENT_CODE - 1,
                                   MAX_SYNC_EVENT_CODE);
  histogram.SetFlags(kUmaTargetedHistogramFlag);
  histogram.Add(code);
}

bool ProfileSyncService::ShouldPushChanges() {
  return ready_to_process_changes_ &&     // Wait for model load and merge.
         !unrecoverable_error_detected_;  // Halt after any terrible events.
}

#endif  // CHROME_PERSONALIZATION
