// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/sync/test_http_bridge_factory.h"

using std::vector;
using browser_sync::ChangeProcessingInterface;
using browser_sync::ModelAssociator;
using browser_sync::SyncBackendHost;
using browser_sync::TestHttpBridgeFactory;

class TestModelAssociator : public ModelAssociator {
 public:
  explicit TestModelAssociator(ProfileSyncService* service)
      : ModelAssociator(service) {
  }

  virtual bool GetSyncIdForTaggedNode(const string16& tag, int64* sync_id) {
    sync_api::WriteTransaction trans(
        sync_service()->backend()->GetUserShareHandle());
    sync_api::ReadNode root(&trans);
    root.InitByRootLookup();

    // First, try to find a node with the title among the root's children.
    // This will be the case if we are testing model persistence, and
    // are reloading a sync repository created earlier in the test.
    int64 last_child_id = sync_api::kInvalidId;
    for (int64 id = root.GetFirstChildId(); id != sync_api::kInvalidId; /***/) {
      sync_api::ReadNode child(&trans);
      child.InitByIdLookup(id);
      last_child_id = id;
      if (tag == child.GetTitle()) {
        *sync_id = id;
        return true;
      }
      id = child.GetSuccessorId();
    }

    sync_api::ReadNode predecessor_node(&trans);
    sync_api::ReadNode* predecessor = NULL;
    if (last_child_id != sync_api::kInvalidId) {
      predecessor_node.InitByIdLookup(last_child_id);
      predecessor = &predecessor_node;
    }
    sync_api::WriteNode node(&trans);
    // Create new fake tagged nodes at the end of the ordering.
    node.InitByCreation(root, predecessor);
    node.SetIsFolder(true);
    node.SetTitle(tag.c_str());
    node.SetExternalId(0);
    *sync_id = node.GetId();
    return true;
  }
};

class TestProfileSyncService : public ProfileSyncService {
 public:
  explicit TestProfileSyncService(Profile* profile)
      : ProfileSyncService(profile) {
    RegisterPreferences();
    SetSyncSetupCompleted();
  }
  virtual ~TestProfileSyncService() {
  }

  virtual void InitializeBackend() {
    set_model_associator(new TestModelAssociator(this));
    TestHttpBridgeFactory* factory = new TestHttpBridgeFactory();
    TestHttpBridgeFactory* factory2 = new TestHttpBridgeFactory();
    backend()->InitializeForTestMode(L"testuser", factory, factory2);
    // The SyncBackend posts a task to the current loop when initialization
    // completes.
    MessageLoop::current()->Run();
    // Initialization is synchronous for test mode, so we should be good to go.
    DCHECK(sync_initialized());
  }

  virtual void OnBackendInitialized() {
    ProfileSyncService::OnBackendInitialized();
    MessageLoop::current()->Quit();
  }

  virtual bool MergeAndSyncAcceptanceNeeded() {
    // Never show the dialog.
    return false;
  }
};

// FakeServerChange constructs a list of sync_api::ChangeRecords while modifying
// the sync model, and can pass the ChangeRecord list to a
// sync_api::SyncObserver (i.e., the ProfileSyncService) to test the client
// change-application behavior.
// Tests using FakeServerChange should be careful to avoid back-references,
// since FakeServerChange will send the edits in the order specified.
class FakeServerChange {
 public:
  explicit FakeServerChange(sync_api::WriteTransaction* trans) : trans_(trans) {
  }

  // Pretend that the server told the syncer to add a bookmark object.
  int64 Add(const std::wstring& title,
            const std::wstring& url,
            bool is_folder,
            int64 parent_id,
            int64 predecessor_id) {
    sync_api::ReadNode parent(trans_);
    EXPECT_TRUE(parent.InitByIdLookup(parent_id));
    sync_api::WriteNode node(trans_);
    if (predecessor_id == 0) {
      EXPECT_TRUE(node.InitByCreation(parent, NULL));
    } else {
      sync_api::ReadNode predecessor(trans_);
      EXPECT_TRUE(predecessor.InitByIdLookup(predecessor_id));
      EXPECT_EQ(predecessor.GetParentId(), parent.GetId());
      EXPECT_TRUE(node.InitByCreation(parent, &predecessor));
    }
    EXPECT_EQ(node.GetPredecessorId(), predecessor_id);
    EXPECT_EQ(node.GetParentId(), parent_id);
    node.SetIsFolder(is_folder);
    node.SetTitle(WideToUTF16(title).c_str());
    if (!is_folder) {
      string16 url16(WideToUTF16(url));
      GURL gurl(url16);
      node.SetURL(url16.c_str());
    }
    sync_api::SyncManager::ChangeRecord record;
    record.action = sync_api::SyncManager::ChangeRecord::ACTION_ADD;
    record.id = node.GetId();
    changes_.push_back(record);
    return node.GetId();
  }

  // Add a bookmark folder.
  int64 AddFolder(const std::wstring& title,
                  int64 parent_id,
                  int64 predecessor_id) {
    return Add(title, std::wstring(), true, parent_id, predecessor_id);
  }

  // Add a bookmark.
  int64 AddURL(const std::wstring& title,
               const std::wstring& url,
               int64 parent_id,
               int64 predecessor_id) {
    return Add(title, url, false, parent_id, predecessor_id);
  }

  // Pretend that the server told the syncer to delete an object.
  void Delete(int64 id) {
    {
      // Delete the sync node.
      sync_api::WriteNode node(trans_);
      EXPECT_TRUE(node.InitByIdLookup(id));
      EXPECT_FALSE(node.GetFirstChildId());
      node.Remove();
    }
    {
      // Verify the deletion.
      sync_api::ReadNode node(trans_);
      EXPECT_FALSE(node.InitByIdLookup(id));
    }

    sync_api::SyncManager::ChangeRecord record;
    record.action = sync_api::SyncManager::ChangeRecord::ACTION_DELETE;
    record.id = id;
    // Deletions are always first in the changelist, but we can't actually do
    // WriteNode::Remove() on the node until its children are moved. So, as
    // a practical matter, users of FakeServerChange must move or delete
    // children before parents.  Thus, we must insert the deletion record
    // at the front of the vector.
    changes_.insert(changes_.begin(), record);
  }

  // Set a new title value, and return the old value.
  std::wstring ModifyTitle(int64 id, const std::wstring& new_title) {
    sync_api::WriteNode node(trans_);
    EXPECT_TRUE(node.InitByIdLookup(id));
    std::wstring old_title = UTF16ToWide(node.GetTitle());
    node.SetTitle(WideToUTF16(new_title).c_str());
    SetModified(id);
    return old_title;
  }

  // Set a new URL value, and return the old value.
  // TODO(ncarter): Determine if URL modifications are even legal.
  std::wstring ModifyURL(int64 id, const std::wstring& new_url) {
    sync_api::WriteNode node(trans_);
    EXPECT_TRUE(node.InitByIdLookup(id));
    EXPECT_FALSE(node.GetIsFolder());
    std::wstring old_url = UTF16ToWide(node.GetURL());
    node.SetURL(WideToUTF16(new_url).c_str());
    SetModified(id);
    return old_url;
  }

  // Set a new parent and predecessor value.  Return the old parent id.
  // We could return the old predecessor id, but it turns out not to be
  // very useful for assertions.
  int64 ModifyPosition(int64 id, int64 parent_id, int64 predecessor_id) {
    sync_api::ReadNode parent(trans_);
    EXPECT_TRUE(parent.InitByIdLookup(parent_id));
    sync_api::WriteNode node(trans_);
    EXPECT_TRUE(node.InitByIdLookup(id));
    int64 old_parent_id = node.GetParentId();
    if (predecessor_id == 0) {
      EXPECT_TRUE(node.SetPosition(parent, NULL));
    } else {
      sync_api::ReadNode predecessor(trans_);
      EXPECT_TRUE(predecessor.InitByIdLookup(predecessor_id));
      EXPECT_EQ(predecessor.GetParentId(), parent.GetId());
      EXPECT_TRUE(node.SetPosition(parent, &predecessor));
    }
    SetModified(id);
    return old_parent_id;
  }

  // Pass the fake change list to |service|.
  void ApplyPendingChanges(browser_sync::ChangeProcessingInterface* processor) {
    processor->ApplyChangesFromSyncModel(trans_,
        changes_.size() ? &changes_[0] : NULL, changes_.size());
  }

  const vector<sync_api::SyncManager::ChangeRecord>& changes() {
    return changes_;
  }

 private:
  // Helper function to push an ACTION_UPDATE record onto the back
  // of the changelist.
  void SetModified(int64 id) {
    // Coalesce multi-property edits.
    if (changes_.size() > 0 && changes_.back().id == id &&
        changes_.back().action ==
        sync_api::SyncManager::ChangeRecord::ACTION_UPDATE)
      return;
    sync_api::SyncManager::ChangeRecord record;
    record.action = sync_api::SyncManager::ChangeRecord::ACTION_UPDATE;
    record.id = id;
    changes_.push_back(record);
  }

  // The transaction on which everything happens.
  sync_api::WriteTransaction *trans_;

  // The change list we construct.
  vector<sync_api::SyncManager::ChangeRecord> changes_;
};

class ProfileSyncServiceTest : public testing::Test {
 protected:
  enum LoadOption { LOAD_FROM_STORAGE, DELETE_EXISTING_STORAGE };
  enum SaveOption { SAVE_TO_STORAGE, DONT_SAVE_TO_STORAGE };
  ProfileSyncServiceTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        file_thread_(ChromeThread::FILE, &message_loop_),
        model_(NULL) {
    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
  }
  virtual ~ProfileSyncServiceTest() {
    // Kill the service before the profile.
    service_.reset();
    profile_.reset();
  }

  ModelAssociator* associator() {
    DCHECK(service_.get());
    return service_->model_associator_;
  }

  ChangeProcessingInterface* change_processor() {
    DCHECK(service_.get());
    return service_->change_processor_.get();
  }

  void StartSyncService() {
    if (!service_.get()) {
      service_.reset(new TestProfileSyncService(profile_.get()));
      service_->Initialize();
    }
    // The service may have already started sync automatically if it's already
    // enabled by user once.
    if (!service_->HasSyncSetupCompleted())
      service_->EnableForUser();
  }
  void StopSyncService(SaveOption save) {
    if (save == DONT_SAVE_TO_STORAGE)
      service_->DisableForUser();
    service_.reset();
  }

  // Load (or re-load) the bookmark model.  |load| controls use of the
  // bookmarks file on disk.  |save| controls whether the newly loaded
  // bookmark model will write out a bookmark file as it goes.
  void LoadBookmarkModel(LoadOption load, SaveOption save) {
    bool delete_bookmarks = load == DELETE_EXISTING_STORAGE;
    profile_->CreateBookmarkModel(delete_bookmarks);
    model_ = profile_->GetBookmarkModel();
    // Wait for the bookmarks model to load.
    profile_->BlockUntilBookmarkModelLoaded();
    // This noticeably speeds up the unit tests that request it.
    if (save == DONT_SAVE_TO_STORAGE)
      model_->ClearStore();
  }

  void ExpectSyncerNodeMatching(sync_api::BaseTransaction* trans,
                                const BookmarkNode* bnode) {
    sync_api::ReadNode gnode(trans);
    EXPECT_TRUE(associator()->InitSyncNodeFromBookmarkId(bnode->id(), &gnode));
    // Non-root node titles and parents must match.
    if (bnode != model_->GetBookmarkBarNode() &&
        bnode != model_->other_node()) {
      EXPECT_EQ(bnode->GetTitle(), UTF16ToWide(gnode.GetTitle()));
      EXPECT_EQ(associator()->GetBookmarkNodeFromSyncId(gnode.GetParentId()),
        bnode->GetParent());
    }
    EXPECT_EQ(bnode->is_folder(), gnode.GetIsFolder());
    if (bnode->is_url())
      EXPECT_EQ(bnode->GetURL(), GURL(gnode.GetURL()));

    // Check for position matches.
    int browser_index = bnode->GetParent()->IndexOfChild(bnode);
    if (browser_index == 0) {
      EXPECT_EQ(gnode.GetPredecessorId(), 0);
    } else {
      const BookmarkNode* bprev =
          bnode->GetParent()->GetChild(browser_index - 1);
      sync_api::ReadNode gprev(trans);
      ASSERT_TRUE(associator()->InitSyncNodeFromBookmarkId(bprev->id(),
                                                           &gprev));
      EXPECT_EQ(gnode.GetPredecessorId(), gprev.GetId());
      EXPECT_EQ(gnode.GetParentId(), gprev.GetParentId());
    }
    if (browser_index == bnode->GetParent()->GetChildCount() - 1) {
      EXPECT_EQ(gnode.GetSuccessorId(), 0);
    } else {
      const BookmarkNode* bnext =
          bnode->GetParent()->GetChild(browser_index + 1);
      sync_api::ReadNode gnext(trans);
      ASSERT_TRUE(associator()->InitSyncNodeFromBookmarkId(bnext->id(),
                                                           &gnext));
      EXPECT_EQ(gnode.GetSuccessorId(), gnext.GetId());
      EXPECT_EQ(gnode.GetParentId(), gnext.GetParentId());
    }
    if (bnode->GetChildCount()) {
      EXPECT_TRUE(gnode.GetFirstChildId());
    }
  }

  void ExpectSyncerNodeMatching(const BookmarkNode* bnode) {
    sync_api::ReadTransaction trans(service_->backend_->GetUserShareHandle());
    ExpectSyncerNodeMatching(&trans, bnode);
  }

  void ExpectBrowserNodeMatching(sync_api::BaseTransaction* trans,
                                 int64 sync_id) {
    EXPECT_TRUE(sync_id);
    const BookmarkNode* bnode =
        associator()->GetBookmarkNodeFromSyncId(sync_id);
    ASSERT_TRUE(bnode);
    int64 id = associator()->GetSyncIdFromBookmarkId(bnode->id());
    EXPECT_EQ(id, sync_id);
    ExpectSyncerNodeMatching(trans, bnode);
  }

  void ExpectBrowserNodeUnknown(int64 sync_id) {
    EXPECT_FALSE(associator()->GetBookmarkNodeFromSyncId(sync_id));
  }

  void ExpectBrowserNodeKnown(int64 sync_id) {
    EXPECT_TRUE(associator()->GetBookmarkNodeFromSyncId(sync_id));
  }

  void ExpectSyncerNodeKnown(const BookmarkNode* node) {
    int64 sync_id = associator()->GetSyncIdFromBookmarkId(node->id());
    EXPECT_NE(sync_id, sync_api::kInvalidId);
  }

  void ExpectSyncerNodeUnknown(const BookmarkNode* node) {
    int64 sync_id = associator()->GetSyncIdFromBookmarkId(node->id());
    EXPECT_EQ(sync_id, sync_api::kInvalidId);
  }

  void ExpectBrowserNodeTitle(int64 sync_id, const std::wstring& title) {
    const BookmarkNode* bnode =
        associator()->GetBookmarkNodeFromSyncId(sync_id);
    ASSERT_TRUE(bnode);
    EXPECT_EQ(bnode->GetTitle(), title);
  }

  void ExpectBrowserNodeURL(int64 sync_id, const std::string& url) {
    const BookmarkNode* bnode =
        associator()->GetBookmarkNodeFromSyncId(sync_id);
    ASSERT_TRUE(bnode);
    GURL url2(url);
    EXPECT_EQ(url2, bnode->GetURL());
  }

  void ExpectBrowserNodeParent(int64 sync_id, int64 parent_sync_id) {
    const BookmarkNode* node = associator()->GetBookmarkNodeFromSyncId(sync_id);
    ASSERT_TRUE(node);
    const BookmarkNode* parent =
        associator()->GetBookmarkNodeFromSyncId(parent_sync_id);
    EXPECT_TRUE(parent);
    EXPECT_EQ(node->GetParent(), parent);
  }

  void ExpectModelMatch(sync_api::BaseTransaction* trans) {
    const BookmarkNode* root = model_->root_node();
    EXPECT_EQ(root->IndexOfChild(model_->GetBookmarkBarNode()), 0);
    EXPECT_EQ(root->IndexOfChild(model_->other_node()), 1);

    std::stack<int64> stack;
    stack.push(bookmark_bar_id());
    while (!stack.empty()) {
      int64 id = stack.top();
      stack.pop();
      if (!id) continue;

      ExpectBrowserNodeMatching(trans, id);

      sync_api::ReadNode gnode(trans);
      ASSERT_TRUE(gnode.InitByIdLookup(id));
      stack.push(gnode.GetFirstChildId());
      stack.push(gnode.GetSuccessorId());
    }
  }

  void ExpectModelMatch() {
    sync_api::ReadTransaction trans(service_->backend_->GetUserShareHandle());
    ExpectModelMatch(&trans);
  }

  int64 other_bookmarks_id() {
    return associator()->GetSyncIdFromBookmarkId(model_->other_node()->id());
  }

  int64 bookmark_bar_id() {
    return associator()->GetSyncIdFromBookmarkId(
        model_->GetBookmarkBarNode()->id());
  }

  SyncBackendHost* backend() { return service_->backend_.get(); }

  // This serves as the "UI loop" on which the ProfileSyncService lives and
  // operates. It is needed because the SyncBackend can post tasks back to
  // the service, meaning it can't be null. It doesn't have to be running,
  // though -- OnInitializationCompleted is the only example (so far) in this
  // test where we need to Run the loop to swallow a task and then quit, to
  // avoid leaking the ProfileSyncService (the PostTask will retain the callee
  // and caller until the task is run).
  MessageLoop message_loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;

  scoped_ptr<ProfileSyncService> service_;
  scoped_ptr<TestingProfile> profile_;
  BookmarkModel* model_;
};

TEST_F(ProfileSyncServiceTest, InitialState) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  EXPECT_TRUE(other_bookmarks_id());
  EXPECT_TRUE(bookmark_bar_id());

  ExpectModelMatch();
}

TEST_F(ProfileSyncServiceTest, BookmarkModelOperations) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  // Test addition.
  const BookmarkNode* folder =
      model_->AddGroup(model_->other_node(), 0, L"foobar");
  ExpectSyncerNodeMatching(folder);
  ExpectModelMatch();
  const BookmarkNode* folder2 = model_->AddGroup(folder, 0, L"nested");
  ExpectSyncerNodeMatching(folder2);
  ExpectModelMatch();
  const BookmarkNode* url1 = model_->AddURL(
      folder, 0, L"Internets #1 Pies Site", GURL("http://www.easypie.com/"));
  ExpectSyncerNodeMatching(url1);
  ExpectModelMatch();
  const BookmarkNode* url2 = model_->AddURL(
      folder, 1, L"Airplanes", GURL("http://www.easyjet.com/"));
  ExpectSyncerNodeMatching(url2);
  ExpectModelMatch();

  // Test modification.
  model_->SetTitle(url2, L"EasyJet");
  ExpectModelMatch();
  model_->Move(url1, folder2, 0);
  ExpectModelMatch();
  model_->Move(folder2, model_->GetBookmarkBarNode(), 0);
  ExpectModelMatch();
  model_->SetTitle(folder2, L"Not Nested");
  ExpectModelMatch();
  model_->Move(folder, folder2, 0);
  ExpectModelMatch();
  model_->SetTitle(folder, L"who's nested now?");
  ExpectModelMatch();

  // Test deletion.
  // Delete a single item.
  model_->Remove(url2->GetParent(), url2->GetParent()->IndexOfChild(url2));
  ExpectModelMatch();
  // Delete an item with several children.
  model_->Remove(folder2->GetParent(),
                 folder2->GetParent()->IndexOfChild(folder2));
  ExpectModelMatch();
}

TEST_F(ProfileSyncServiceTest, ServerChangeProcessing) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  sync_api::WriteTransaction trans(backend()->GetUserShareHandle());

  FakeServerChange adds(&trans);
  int64 f1 = adds.AddFolder(L"Server Folder B", bookmark_bar_id(), 0);
  int64 f2 = adds.AddFolder(L"Server Folder A", bookmark_bar_id(), f1);
  int64 u1 = adds.AddURL(L"Some old site", L"ftp://nifty.andrew.cmu.edu/",
                         bookmark_bar_id(), f2);
  int64 u2 = adds.AddURL(L"Nifty", L"ftp://nifty.andrew.cmu.edu/", f1, 0);
  // u3 is a duplicate URL
  int64 u3 = adds.AddURL(L"Nifty2", L"ftp://nifty.andrew.cmu.edu/", f1, u2);
  // u4 is a duplicate title, different URL.
  adds.AddURL(L"Some old site", L"http://slog.thestranger.com/",
              bookmark_bar_id(), u1);
  // u5 tests an empty-string title.
  std::wstring javascript_url(L"javascript:(function(){var w=window.open(" \
                         L"'about:blank','gnotesWin','location=0,menubar=0," \
                         L"scrollbars=0,status=0,toolbar=0,width=300," \
                         L"height=300,resizable');});");
  adds.AddURL(L"", javascript_url, other_bookmarks_id(), 0);

  vector<sync_api::SyncManager::ChangeRecord>::const_iterator it;
  // The bookmark model shouldn't yet have seen any of the nodes of |adds|.
  for (it = adds.changes().begin(); it != adds.changes().end(); ++it)
    ExpectBrowserNodeUnknown(it->id);

  adds.ApplyPendingChanges(change_processor());

  // Make sure the bookmark model received all of the nodes in |adds|.
  for (it = adds.changes().begin(); it != adds.changes().end(); ++it)
    ExpectBrowserNodeMatching(&trans, it->id);
  ExpectModelMatch(&trans);

  // Part two: test modifications.
  FakeServerChange mods(&trans);
  // Mess with u2, and move it into empty folder f2
  // TODO(ncarter): Determine if we allow ModifyURL ops or not.
  /* std::wstring u2_old_url = mods.ModifyURL(u2, L"http://www.google.com"); */
  std::wstring u2_old_title = mods.ModifyTitle(u2, L"The Google");
  int64 u2_old_parent = mods.ModifyPosition(u2, f2, 0);

  // Now move f1 after u2.
  std::wstring f1_old_title = mods.ModifyTitle(f1, L"Server Folder C");
  int64 f1_old_parent = mods.ModifyPosition(f1, f2, u2);

  // Then add u3 after f1.
  int64 u3_old_parent = mods.ModifyPosition(u3, f2, f1);

  // Test that the property changes have not yet taken effect.
  ExpectBrowserNodeTitle(u2, u2_old_title);
  /* ExpectBrowserNodeURL(u2, u2_old_url); */
  ExpectBrowserNodeParent(u2, u2_old_parent);

  ExpectBrowserNodeTitle(f1, f1_old_title);
  ExpectBrowserNodeParent(f1, f1_old_parent);

  ExpectBrowserNodeParent(u3, u3_old_parent);

  // Apply the changes.
  mods.ApplyPendingChanges(change_processor());

  // Check for successful application.
  for (it = mods.changes().begin(); it != mods.changes().end(); ++it)
    ExpectBrowserNodeMatching(&trans, it->id);
  ExpectModelMatch(&trans);

  // Part 3: Test URL deletion.
  FakeServerChange dels(&trans);
  dels.Delete(u2);
  dels.Delete(u3);

  ExpectBrowserNodeKnown(u2);
  ExpectBrowserNodeKnown(u3);

  dels.ApplyPendingChanges(change_processor());

  ExpectBrowserNodeUnknown(u2);
  ExpectBrowserNodeUnknown(u3);
  ExpectModelMatch(&trans);
}

// Tests a specific case in ApplyModelChanges where we move the
// children out from under a parent, and then delete the parent
// in the same changelist.  The delete shows up first in the changelist,
// requiring the children to be moved to a temporary location.
TEST_F(ProfileSyncServiceTest, ServerChangeRequiringFosterParent) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  sync_api::WriteTransaction trans(backend()->GetUserShareHandle());

  // Stress the immediate children of other_node because that's where
  // ApplyModelChanges puts a temporary foster parent node.
  std::wstring url(L"http://dev.chromium.org/");
  FakeServerChange adds(&trans);
  int64 f0 = other_bookmarks_id();                 // + other_node
  int64 f1 = adds.AddFolder(L"f1",      f0, 0);    //   + f1
  int64 f2 = adds.AddFolder(L"f2",      f1, 0);    //     + f2
  int64 u3 = adds.AddURL(   L"u3", url, f2, 0);    //       + u3
  int64 u4 = adds.AddURL(   L"u4", url, f2, u3);   //       + u4
  int64 u5 = adds.AddURL(   L"u5", url, f1, f2);   //     + u5
  int64 f6 = adds.AddFolder(L"f6",      f1, u5);   //     + f6
  int64 u7 = adds.AddURL(   L"u7", url, f0, f1);   //   + u7

  vector<sync_api::SyncManager::ChangeRecord>::const_iterator it;
  // The bookmark model shouldn't yet have seen any of the nodes of |adds|.
  for (it = adds.changes().begin(); it != adds.changes().end(); ++it)
    ExpectBrowserNodeUnknown(it->id);

  adds.ApplyPendingChanges(change_processor());

  // Make sure the bookmark model received all of the nodes in |adds|.
  for (it = adds.changes().begin(); it != adds.changes().end(); ++it)
    ExpectBrowserNodeMatching(&trans, it->id);
  ExpectModelMatch(&trans);

  // We have to do the moves before the deletions, but FakeServerChange will
  // put the deletion at the front of the changelist.
  FakeServerChange ops(&trans);
  ops.ModifyPosition(f6, other_bookmarks_id(), 0);
  ops.ModifyPosition(u3, other_bookmarks_id(), f1);  // Prev == f1 is OK here.
  ops.ModifyPosition(f2, other_bookmarks_id(), u7);
  ops.ModifyPosition(u7, f2, 0);
  ops.ModifyPosition(u4, other_bookmarks_id(), f2);
  ops.ModifyPosition(u5, f6, 0);
  ops.Delete(f1);

  ops.ApplyPendingChanges(change_processor());

  ExpectModelMatch(&trans);
}

// Simulate a server change record containing a valid but non-canonical URL.
TEST_F(ProfileSyncServiceTest, ServerChangeWithNonCanonicalURL) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());

    FakeServerChange adds(&trans);
    std::string url("http://dev.chromium.org");
    EXPECT_NE(GURL(url).spec(), url);
    adds.AddURL(L"u1", UTF8ToWide(url), other_bookmarks_id(), 0);

    adds.ApplyPendingChanges(change_processor());

    EXPECT_TRUE(model_->other_node()->GetChildCount() == 1);
    ExpectModelMatch(&trans);
  }

  // Now reboot the sync service, forcing a merge step.
  StopSyncService(SAVE_TO_STORAGE);
  LoadBookmarkModel(LOAD_FROM_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  // There should still be just the one bookmark.
  EXPECT_TRUE(model_->other_node()->GetChildCount() == 1);
  ExpectModelMatch();
}

// Simulate a server change record containing an invalid URL (per GURL).
// TODO(ncarter): Disabled due to crashes.  Fix bug 1677563.
TEST_F(ProfileSyncServiceTest, DISABLED_ServerChangeWithInvalidURL) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  int child_count = 0;
  {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());

    FakeServerChange adds(&trans);
    EXPECT_FALSE(GURL("x").is_valid());
    adds.AddURL(L"u1", L"x", other_bookmarks_id(), 0);

    adds.ApplyPendingChanges(change_processor());

    // We're lenient about what should happen -- the model could wind up with
    // the node or without it; but things should be consistent, and we
    // shouldn't crash.
    child_count = model_->other_node()->GetChildCount();
    EXPECT_TRUE(child_count == 0 || child_count == 1);
    ExpectModelMatch(&trans);
  }

  // Now reboot the sync service, forcing a merge step.
  StopSyncService(SAVE_TO_STORAGE);
  LoadBookmarkModel(LOAD_FROM_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  // Things ought not to have changed.
  EXPECT_EQ(model_->other_node()->GetChildCount(), child_count);
  ExpectModelMatch();
}

// Test strings that might pose a problem if the titles ever became used as
// file names in the sync backend.
TEST_F(ProfileSyncServiceTest, CornerCaseNames) {
  // TODO(ncarter): Bug 1570238 explains the failure of this test.
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  const wchar_t* names[] = {
      // The empty string.
      L"",
      // Illegal Windows filenames.
      L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4",
      L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3",
      L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9",
      // Current/parent directory markers.
      L".", L"..", L"...",
      // Files created automatically by the Windows shell.
      L"Thumbs.db", L".DS_Store",
      // Names including Win32-illegal characters, and path separators.
      L"foo/bar", L"foo\\bar", L"foo?bar", L"foo:bar", L"foo|bar", L"foo\"bar",
      L"foo'bar", L"foo<bar", L"foo>bar", L"foo%bar", L"foo*bar", L"foo]bar",
      L"foo[bar",
  };
  // Create both folders and bookmarks using each name.
  GURL url("http://www.doublemint.com");
  for (size_t i = 0; i < arraysize(names); ++i) {
    model_->AddGroup(model_->other_node(), 0, names[i]);
    model_->AddURL(model_->other_node(), 0, names[i], url);
  }

  // Verify that the browser model matches the sync model.
  EXPECT_TRUE(model_->other_node()->GetChildCount() == 2*arraysize(names));
  ExpectModelMatch();
}

// Stress the internal representation of position by sparse numbers. We want
// to repeatedly bisect the range of available positions, to force the
// syncer code to renumber its ranges.  Pick a number big enough so that it
// would exhaust 32bits of room between items a couple of times.
TEST_F(ProfileSyncServiceTest, RepeatedMiddleInsertion) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  static const int kTimesToInsert = 256;

  // Create two book-end nodes to insert between.
  model_->AddGroup(model_->other_node(), 0, L"Alpha");
  model_->AddGroup(model_->other_node(), 1, L"Omega");
  int count = 2;

  // Test insertion in first half of range by repeatedly inserting in second
  // position.
  for (int i = 0; i < kTimesToInsert; ++i) {
    std::wstring title = std::wstring(L"Pre-insertion ") + IntToWString(i);
    model_->AddGroup(model_->other_node(), 1, title);
    count++;
  }

  // Test insertion in second half of range by repeatedly inserting in
  // second-to-last position.
  for (int i = 0; i < kTimesToInsert; ++i) {
    std::wstring title = std::wstring(L"Post-insertion ") + IntToWString(i);
    model_->AddGroup(model_->other_node(), count - 1, title);
    count++;
  }

  // Verify that the browser model matches the sync model.
  EXPECT_EQ(model_->other_node()->GetChildCount(), count);
  ExpectModelMatch();
}

// Introduce a consistency violation into the model, and see that it
// puts itself into a lame, error state.
TEST_F(ProfileSyncServiceTest, UnrecoverableErrorSuspendsService) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  // Synchronization should be up and running at this point.
  EXPECT_TRUE(service_->ShouldPushChanges());

  // Add a node which will be the target of the consistency violation.
  const BookmarkNode* node =
      model_->AddGroup(model_->other_node(), 0, L"node");
  ExpectSyncerNodeMatching(node);

  // Now destroy the syncer node as if we were the ProfileSyncService without
  // updating the ProfileSyncService state.  This should introduce
  // inconsistency between the two models.
  {
    sync_api::WriteTransaction trans(service_->backend_->GetUserShareHandle());
    sync_api::WriteNode sync_node(&trans);
    EXPECT_TRUE(associator()->InitSyncNodeFromBookmarkId(node->id(),
                                                         &sync_node));
    sync_node.Remove();
  }
  // The models don't match at this point, but the ProfileSyncService
  // doesn't know it yet.
  ExpectSyncerNodeKnown(node);
  EXPECT_TRUE(service_->ShouldPushChanges());

  // Add a child to the inconsistent node.  This should cause detection of the
  // problem.
  const BookmarkNode* nested = model_->AddGroup(node, 0, L"nested");
  EXPECT_FALSE(service_->ShouldPushChanges());
  ExpectSyncerNodeUnknown(nested);

  // Try to add a node under a totally different parent.  This should also
  // fail -- the ProfileSyncService should stop processing changes after
  // encountering a consistency violation.
  const BookmarkNode* unrelated = model_->AddGroup(
      model_->GetBookmarkBarNode(), 0, L"unrelated");
  EXPECT_FALSE(service_->ShouldPushChanges());
  ExpectSyncerNodeUnknown(unrelated);

  // TODO(ncarter): We ought to test the ProfileSyncService state machine
  // directly here once that's formalized and exposed.
}

struct TestData {
  const wchar_t* title;
  const char* url;
};

// TODO(ncarter): Integrate the existing TestNode/PopulateNodeFromString code
// in the bookmark model unittest, to make it simpler to set up test data
// here (and reduce the amount of duplication among tests), and to reduce the
// duplication.
class ProfileSyncServiceTestWithData : public ProfileSyncServiceTest {
 protected:
  // Populates or compares children of the given bookmark node from/with the
  // given test data array with the given size.
  void PopulateFromTestData(const BookmarkNode* node,
                            const TestData* data,
                            int size);
  void CompareWithTestData(const BookmarkNode* node,
                           const TestData* data,
                           int size);

  void ExpectBookmarkModelMatchesTestData();
  void WriteTestDataToBookmarkModel();
};

namespace {

// Constants for bookmark model that looks like:
// |-- Bookmark bar
// |   |-- u2, http://www.u2.com/
// |   |-- f1
// |   |   |-- f1u4, http://www.f1u4.com/
// |   |   |-- f1u2, http://www.f1u2.com/
// |   |   |-- f1u3, http://www.f1u3.com/
// |   |   +-- f1u1, http://www.f1u1.com/
// |   |-- u1, http://www.u1.com/
// |   +-- f2
// |       |-- f2u2, http://www.f2u2.com/
// |       |-- f2u4, http://www.f2u4.com/
// |       |-- f2u3, http://www.f2u3.com/
// |       +-- f2u1, http://www.f2u1.com/
// +-- Other bookmarks
//     |-- f3
//     |   |-- f3u4, http://www.f3u4.com/
//     |   |-- f3u2, http://www.f3u2.com/
//     |   |-- f3u3, http://www.f3u3.com/
//     |   +-- f3u1, http://www.f3u1.com/
//     |-- u4, http://www.u4.com/
//     |-- u3, http://www.u3.com/
//     --- f4
//     |   |-- f4u1, http://www.f4u1.com/
//     |   |-- f4u2, http://www.f4u2.com/
//     |   |-- f4u3, http://www.f4u3.com/
//     |   +-- f4u4, http://www.f4u4.com/
//     |-- dup
//     |   +-- dupu1, http://www.dupu1.com/
//     +-- dup
//         +-- dupu2, http://www.dupu1.com/
//
static TestData kBookmarkBarChildren[] = {
  { L"u2", "http://www.u2.com/" },
  { L"f1", NULL },
  { L"u1", "http://www.u1.com/" },
  { L"f2", NULL },
};
static TestData kF1Children[] = {
  { L"f1u4", "http://www.f1u4.com/" },
  { L"f1u2", "http://www.f1u2.com/" },
  { L"f1u3", "http://www.f1u3.com/" },
  { L"f1u1", "http://www.f1u1.com/" },
};
static TestData kF2Children[] = {
  { L"f2u2", "http://www.f2u2.com/" },
  { L"f2u4", "http://www.f2u4.com/" },
  { L"f2u3", "http://www.f2u3.com/" },
  { L"f2u1", "http://www.f2u1.com/" },
};

static TestData kOtherBookmarksChildren[] = {
  { L"f3", NULL },
  { L"u4", "http://www.u4.com/" },
  { L"u3", "http://www.u3.com/" },
  { L"f4", NULL },
  { L"dup", NULL },
  { L"dup", NULL },
};
static TestData kF3Children[] = {
  { L"f3u4", "http://www.f3u4.com/" },
  { L"f3u2", "http://www.f3u2.com/" },
  { L"f3u3", "http://www.f3u3.com/" },
  { L"f3u1", "http://www.f3u1.com/" },
};
static TestData kF4Children[] = {
  { L"f4u1", "http://www.f4u1.com/" },
  { L"f4u2", "http://www.f4u2.com/" },
  { L"f4u3", "http://www.f4u3.com/" },
  { L"f4u4", "http://www.f4u4.com/" },
};
static TestData kDup1Children[] = {
  { L"dupu1", "http://www.dupu1.com/" },
};
static TestData kDup2Children[] = {
  { L"dupu2", "http://www.dupu2.com/" },
};

}  // anonymous namespace.

void ProfileSyncServiceTestWithData::PopulateFromTestData(
    const BookmarkNode* node, const TestData* data, int size) {
  DCHECK(node);
  DCHECK(data);
  DCHECK(node->is_folder());
  for (int i = 0; i < size; ++i) {
    const TestData& item = data[i];
    if (item.url) {
      model_->AddURL(node, i, item.title, GURL(item.url));
    } else {
      model_->AddGroup(node, i, item.title);
    }
  }
}

void ProfileSyncServiceTestWithData::CompareWithTestData(
    const BookmarkNode* node, const TestData* data, int size) {
  DCHECK(node);
  DCHECK(data);
  DCHECK(node->is_folder());
  for (int i = 0; i < size; ++i) {
    const BookmarkNode* child_node = node->GetChild(i);
    const TestData& item = data[i];
    EXPECT_TRUE(child_node->GetTitle() == item.title);
    if (item.url) {
      EXPECT_FALSE(child_node->is_folder());
      EXPECT_TRUE(child_node->is_url());
      EXPECT_TRUE(child_node->GetURL() == GURL(item.url));
    } else {
      EXPECT_TRUE(child_node->is_folder());
      EXPECT_FALSE(child_node->is_url());
    }
  }
}

// TODO(munjal): We should implement some way of generating random data and can
// use the same seed to generate the same sequence.
void ProfileSyncServiceTestWithData::WriteTestDataToBookmarkModel() {
  const BookmarkNode* bookmarks_bar_node = model_->GetBookmarkBarNode();
  PopulateFromTestData(bookmarks_bar_node,
                       kBookmarkBarChildren,
                       arraysize(kBookmarkBarChildren));

  ASSERT_GE(bookmarks_bar_node->GetChildCount(), 4);
  const BookmarkNode* f1_node = bookmarks_bar_node->GetChild(1);
  PopulateFromTestData(f1_node, kF1Children, arraysize(kF1Children));
  const BookmarkNode* f2_node = bookmarks_bar_node->GetChild(3);
  PopulateFromTestData(f2_node, kF2Children, arraysize(kF2Children));

  const BookmarkNode* other_bookmarks_node = model_->other_node();
  PopulateFromTestData(other_bookmarks_node,
                       kOtherBookmarksChildren,
                       arraysize(kOtherBookmarksChildren));

  ASSERT_GE(other_bookmarks_node->GetChildCount(), 6);
  const BookmarkNode* f3_node = other_bookmarks_node->GetChild(0);
  PopulateFromTestData(f3_node, kF3Children, arraysize(kF3Children));
  const BookmarkNode* f4_node = other_bookmarks_node->GetChild(3);
  PopulateFromTestData(f4_node, kF4Children, arraysize(kF4Children));
  const BookmarkNode* dup_node = other_bookmarks_node->GetChild(4);
  PopulateFromTestData(dup_node, kDup1Children, arraysize(kDup1Children));
  dup_node = other_bookmarks_node->GetChild(5);
  PopulateFromTestData(dup_node, kDup2Children, arraysize(kDup2Children));

  ExpectBookmarkModelMatchesTestData();
}

void ProfileSyncServiceTestWithData::ExpectBookmarkModelMatchesTestData() {
  const BookmarkNode* bookmark_bar_node = model_->GetBookmarkBarNode();
  CompareWithTestData(bookmark_bar_node,
                      kBookmarkBarChildren,
                      arraysize(kBookmarkBarChildren));

  ASSERT_GE(bookmark_bar_node->GetChildCount(), 4);
  const BookmarkNode* f1_node = bookmark_bar_node->GetChild(1);
  CompareWithTestData(f1_node, kF1Children, arraysize(kF1Children));
  const BookmarkNode* f2_node = bookmark_bar_node->GetChild(3);
  CompareWithTestData(f2_node, kF2Children, arraysize(kF2Children));

  const BookmarkNode* other_bookmarks_node = model_->other_node();
  CompareWithTestData(other_bookmarks_node,
                      kOtherBookmarksChildren,
                      arraysize(kOtherBookmarksChildren));

  ASSERT_GE(other_bookmarks_node->GetChildCount(), 6);
  const BookmarkNode* f3_node = other_bookmarks_node->GetChild(0);
  CompareWithTestData(f3_node, kF3Children, arraysize(kF3Children));
  const BookmarkNode* f4_node = other_bookmarks_node->GetChild(3);
  CompareWithTestData(f4_node, kF4Children, arraysize(kF4Children));
  const BookmarkNode* dup_node = other_bookmarks_node->GetChild(4);
  CompareWithTestData(dup_node, kDup1Children, arraysize(kDup1Children));
  dup_node = other_bookmarks_node->GetChild(5);
  CompareWithTestData(dup_node, kDup2Children, arraysize(kDup2Children));
}

// Tests persistence of the profile sync service by destroying the
// profile sync service and then reloading it from disk.
TEST_F(ProfileSyncServiceTestWithData, Persistence) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  WriteTestDataToBookmarkModel();

  ExpectModelMatch();

  // Force both models to discard their data and reload from disk.  This
  // simulates what would happen if the browser were to shutdown normally,
  // and then relaunch.
  StopSyncService(SAVE_TO_STORAGE);
  LoadBookmarkModel(LOAD_FROM_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  ExpectBookmarkModelMatchesTestData();

  // With the BookmarkModel contents verified, ExpectModelMatch will
  // verify the contents of the sync model.
  ExpectModelMatch();
}

// Tests the merge case when the BookmarkModel is non-empty but the
// sync model is empty.  This corresponds to uploading browser
// bookmarks to an initially empty, new account.
TEST_F(ProfileSyncServiceTestWithData, MergeWithEmptySyncModel) {
  // Don't start the sync service until we've populated the bookmark model.
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, SAVE_TO_STORAGE);

  WriteTestDataToBookmarkModel();

  // Restart the profile sync service.  This should trigger a merge step
  // during initialization -- we expect the browser bookmarks to be written
  // to the sync service during this call.
  StartSyncService();

  // Verify that the bookmark model hasn't changed, and that the sync model
  // matches it exactly.
  ExpectBookmarkModelMatchesTestData();
  ExpectModelMatch();
}

// Tests the merge case when the BookmarkModel is empty but the sync model is
// non-empty.  This corresponds (somewhat) to a clean install of the browser,
// with no bookmarks, connecting to a sync account that has some bookmarks.
TEST_F(ProfileSyncServiceTestWithData, MergeWithEmptyBookmarkModel) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  WriteTestDataToBookmarkModel();

  ExpectModelMatch();

  // Force the sync service to shut down and write itself to disk.
  StopSyncService(SAVE_TO_STORAGE);

  // Blow away the bookmark model -- it should be empty afterwards.
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  EXPECT_EQ(model_->GetBookmarkBarNode()->GetChildCount(), 0);
  EXPECT_EQ(model_->other_node()->GetChildCount(), 0);

  // Now restart the sync service.  Starting it should populate the bookmark
  // model -- test for consistency.
  StartSyncService();
  ExpectBookmarkModelMatchesTestData();
  ExpectModelMatch();
}

// Tests the merge cases when both the models are expected to be identical
// after the merge.
TEST_F(ProfileSyncServiceTestWithData, MergeExpectedIdenticalModels) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();
  WriteTestDataToBookmarkModel();
  ExpectModelMatch();
  StopSyncService(SAVE_TO_STORAGE);

  // At this point both the bookmark model and the server should have the
  // exact same data and it should match the test data.
  LoadBookmarkModel(LOAD_FROM_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();
  ExpectBookmarkModelMatchesTestData();
  ExpectModelMatch();
  StopSyncService(SAVE_TO_STORAGE);

  // Now reorder some bookmarks in the bookmark model and then merge. Make
  // sure we get the order of the server after merge.
  LoadBookmarkModel(LOAD_FROM_STORAGE, DONT_SAVE_TO_STORAGE);
  ExpectBookmarkModelMatchesTestData();
  const BookmarkNode* bookmark_bar = model_->GetBookmarkBarNode();
  ASSERT_TRUE(bookmark_bar);
  ASSERT_GT(bookmark_bar->GetChildCount(), 1);
  model_->Move(bookmark_bar->GetChild(0), bookmark_bar, 1);
  StartSyncService();
  ExpectModelMatch();
  ExpectBookmarkModelMatchesTestData();
}

// Tests the merge cases when both the models are expected to be identical
// after the merge.
TEST_F(ProfileSyncServiceTestWithData, MergeModelsWithSomeExtras) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  WriteTestDataToBookmarkModel();
  ExpectBookmarkModelMatchesTestData();

  // Remove some nodes and reorder some nodes.
  const BookmarkNode* bookmark_bar_node = model_->GetBookmarkBarNode();
  int remove_index = 2;
  ASSERT_GT(bookmark_bar_node->GetChildCount(), remove_index);
  const BookmarkNode* child_node = bookmark_bar_node->GetChild(remove_index);
  ASSERT_TRUE(child_node);
  ASSERT_TRUE(child_node->is_url());
  model_->Remove(bookmark_bar_node, remove_index);
  ASSERT_GT(bookmark_bar_node->GetChildCount(), remove_index);
  child_node = bookmark_bar_node->GetChild(remove_index);
  ASSERT_TRUE(child_node);
  ASSERT_TRUE(child_node->is_folder());
  model_->Remove(bookmark_bar_node, remove_index);

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_GE(other_node->GetChildCount(), 1);
  const BookmarkNode* f3_node = other_node->GetChild(0);
  ASSERT_TRUE(f3_node);
  ASSERT_TRUE(f3_node->is_folder());
  remove_index = 2;
  ASSERT_GT(f3_node->GetChildCount(), remove_index);
  model_->Remove(f3_node, remove_index);
  ASSERT_GT(f3_node->GetChildCount(), remove_index);
  model_->Remove(f3_node, remove_index);

  StartSyncService();
  ExpectModelMatch();
  StopSyncService(SAVE_TO_STORAGE);

  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  WriteTestDataToBookmarkModel();
  ExpectBookmarkModelMatchesTestData();

  // Remove some nodes and reorder some nodes.
  bookmark_bar_node = model_->GetBookmarkBarNode();
  remove_index = 0;
  ASSERT_GT(bookmark_bar_node->GetChildCount(), remove_index);
  child_node = bookmark_bar_node->GetChild(remove_index);
  ASSERT_TRUE(child_node);
  ASSERT_TRUE(child_node->is_url());
  model_->Remove(bookmark_bar_node, remove_index);
  ASSERT_GT(bookmark_bar_node->GetChildCount(), remove_index);
  child_node = bookmark_bar_node->GetChild(remove_index);
  ASSERT_TRUE(child_node);
  ASSERT_TRUE(child_node->is_folder());
  model_->Remove(bookmark_bar_node, remove_index);

  ASSERT_GE(bookmark_bar_node->GetChildCount(), 2);
  model_->Move(bookmark_bar_node->GetChild(0), bookmark_bar_node, 1);

  other_node = model_->other_node();
  ASSERT_GE(other_node->GetChildCount(), 1);
  f3_node = other_node->GetChild(0);
  ASSERT_TRUE(f3_node);
  ASSERT_TRUE(f3_node->is_folder());
  remove_index = 0;
  ASSERT_GT(f3_node->GetChildCount(), remove_index);
  model_->Remove(f3_node, remove_index);
  ASSERT_GT(f3_node->GetChildCount(), remove_index);
  model_->Remove(f3_node, remove_index);

  ASSERT_GE(other_node->GetChildCount(), 4);
  model_->Move(other_node->GetChild(0), other_node, 1);
  model_->Move(other_node->GetChild(2), other_node, 3);

  StartSyncService();
  ExpectModelMatch();

  // After the merge, the model should match the test data.
  ExpectBookmarkModelMatchesTestData();
}

// Tests that when persisted model assocations are used, things work fine.
TEST_F(ProfileSyncServiceTestWithData, ModelAssociationPersistence) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  WriteTestDataToBookmarkModel();
  StartSyncService();
  ExpectModelMatch();
  // Force the sync service to shut down and write itself to disk.
  StopSyncService(SAVE_TO_STORAGE);
  // Now restart the sync service. This time it should use the persistent
  // assocations.
  StartSyncService();
  ExpectModelMatch();
}

// Tests that when persisted model assocations are used, things work fine.
TEST_F(ProfileSyncServiceTestWithData, ModelAssociationInvalidPersistence) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  WriteTestDataToBookmarkModel();
  StartSyncService();
  ExpectModelMatch();
  // Force the sync service to shut down and write itself to disk.
  StopSyncService(SAVE_TO_STORAGE);
  // Change the bookmark model before restarting sync service to simulate
  // the situation where bookmark model is different from sync model and
  // make sure model associator correctly rebuilds associations.
  const BookmarkNode* bookmark_bar_node = model_->GetBookmarkBarNode();
  model_->AddURL(bookmark_bar_node, 0, L"xtra", GURL("http://www.xtra.com"));
  // Now restart the sync service. This time it will try to use the persistent
  // associations and realize that they are invalid and hence will rebuild
  // associations.
  StartSyncService();
  ExpectModelMatch();
}

TEST_F(ProfileSyncServiceTestWithData, SortChildren) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, DONT_SAVE_TO_STORAGE);
  StartSyncService();

  // Write test data to bookmark model and verify that the models match.
  WriteTestDataToBookmarkModel();
  const BookmarkNode* folder_added = model_->other_node()->GetChild(0);
  ASSERT_TRUE(folder_added);
  ASSERT_TRUE(folder_added->is_folder());

  ExpectModelMatch();

  // Sort the other-bookmarks children and expect that hte models match.
  model_->SortChildren(folder_added);
  ExpectModelMatch();
}

// See what happens if we enable sync but then delete the "Sync Data"
// folder.
TEST_F(ProfileSyncServiceTestWithData, RecoverAfterDeletingSyncDataDirectory) {
  LoadBookmarkModel(DELETE_EXISTING_STORAGE, SAVE_TO_STORAGE);
  StartSyncService();

  WriteTestDataToBookmarkModel();

  // While the service is running.
  FilePath sync_data_directory = backend()->sync_data_folder_path();

  // Simulate a normal shutdown for the sync service (don't disable it for
  // the user, which would reset the preferences and delete the sync data
  // directory).
  StopSyncService(SAVE_TO_STORAGE);

  // Now pretend that the user has deleted this directory from the disk.
  file_util::Delete(sync_data_directory, true);

  // Restart the sync service.
  StartSyncService();

  // Make sure we're back in sync.  In real life, the user would need
  // to reauthenticate before this happens, but in the test, authentication
  // is sidestepped.
  ExpectBookmarkModelMatchesTestData();
  ExpectModelMatch();
}
