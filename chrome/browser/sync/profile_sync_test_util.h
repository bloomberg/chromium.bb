// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/sync/test_http_bridge_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

ACTION_P(Notify, type) {
  NotificationService::current()->Notify(type,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->Quit();
}

class TestModelAssociatorHelper {
 public:
  explicit TestModelAssociatorHelper(browser_sync::TestIdFactory* id_factory)
      : id_factory_(id_factory) {
  }

  template <class ModelAssociatorImpl>
  bool GetSyncIdForTaggedNode(ModelAssociatorImpl* associator,
                              const std::string& tag, int64* sync_id) {
    std::wstring tag_wide;
    if (!UTF8ToWide(tag.c_str(), tag.length(), &tag_wide)) {
      NOTREACHED() << "Unable to convert UTF8 to wide for string: " << tag;
      return false;
    }

    browser_sync::SyncBackendHost::UserShareHandle share(
        associator->sync_service()->backend()->GetUserShareHandle());
    bool root_exists = false;
    ModelType type = ModelAssociatorImpl::model_type();
    {
      sync_api::WriteTransaction trans(share);
      sync_api::ReadNode uber_root(&trans);
      uber_root.InitByRootLookup();

      sync_api::ReadNode root(&trans);
      root_exists = root.InitByTagLookup(
          ProfileSyncServiceTestHelper::GetTagForType(type));
    }

    if (!root_exists) {
      bool created = ProfileSyncServiceTestHelper::CreateRoot(
          type,
          associator->sync_service(),
          id_factory_);
      if (!created)
        return false;
    }

    sync_api::WriteTransaction trans(share);
    sync_api::ReadNode root(&trans);
    EXPECT_TRUE(root.InitByTagLookup(
        ProfileSyncServiceTestHelper::GetTagForType(type)));

    // First, try to find a node with the title among the root's children.
    // This will be the case if we are testing model persistence, and
    // are reloading a sync repository created earlier in the test.
    int64 last_child_id = sync_api::kInvalidId;
    for (int64 id = root.GetFirstChildId(); id != sync_api::kInvalidId; /***/) {
      sync_api::ReadNode child(&trans);
      child.InitByIdLookup(id);
      last_child_id = id;
      if (tag_wide == child.GetTitle()) {
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
    node.InitByCreation(ModelAssociatorImpl::model_type(), root, predecessor);
    node.SetIsFolder(true);
    node.SetTitle(tag_wide);
    node.SetExternalId(0);
    *sync_id = node.GetId();
    return true;
  }

  ~TestModelAssociatorHelper() {}
 private:
  browser_sync::TestIdFactory* id_factory_;
};

class ProfileSyncServiceObserverMock : public ProfileSyncServiceObserver {
 public:
  MOCK_METHOD0(OnStateChanged, void());
};

class ThreadNotificationService
    : public base::RefCountedThreadSafe<ThreadNotificationService> {
 public:
  explicit ThreadNotificationService(base::Thread* notification_thread)
      : done_event_(false, false),
      notification_thread_(notification_thread) {}

  void Init() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    notification_thread_->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ThreadNotificationService::InitTask));
    done_event_.Wait();
  }

  void TearDown() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    notification_thread_->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &ThreadNotificationService::TearDownTask));
    done_event_.Wait();
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadNotificationService>;

  void InitTask() {
    service_.reset(new NotificationService());
    done_event_.Signal();
  }

  void TearDownTask() {
    service_.reset(NULL);
    done_event_.Signal();
  }

  base::WaitableEvent done_event_;
  base::Thread* notification_thread_;
  scoped_ptr<NotificationService> service_;
};

class ThreadNotifier :  // NOLINT
    public base::RefCountedThreadSafe<ThreadNotifier> {
 public:
  explicit ThreadNotifier(base::Thread* notify_thread)
      : done_event_(false, false),
        notify_thread_(notify_thread) {}

  void Notify(NotificationType type, const NotificationDetails& details) {
    Notify(type, NotificationService::AllSources(), details);
  }

  void Notify(NotificationType type,
              const NotificationSource& source,
              const NotificationDetails& details) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    notify_thread_->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &ThreadNotifier::NotifyTask,
                          type,
                          source,
                          details));
    done_event_.Wait();
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadNotifier>;

  void NotifyTask(NotificationType type,
                  const NotificationSource& source,
                  const NotificationDetails& details) {
    NotificationService::current()->Notify(type, source, details);
    done_event_.Signal();
  }

  base::WaitableEvent done_event_;
  base::Thread* notify_thread_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
