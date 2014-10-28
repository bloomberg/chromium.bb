// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"
#include "chrome/browser/chromeos/file_system_provider/observed_entry.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "camera-pictures";
const char kDisplayName[] = "Camera Pictures";
const base::FilePath::CharType kDirectoryPath[] = "/hello/world";

// Fake implementation of the event router, mocking out a real extension.
// Handles requests and replies with fake answers back to the file system via
// the request manager.
class FakeEventRouter : public extensions::EventRouter {
 public:
  FakeEventRouter(Profile* profile, ProvidedFileSystemInterface* file_system)
      : EventRouter(profile, NULL),
        file_system_(file_system),
        reply_result_(base::File::FILE_OK) {}
  virtual ~FakeEventRouter() {}

  // Handles an event which would normally be routed to an extension. Instead
  // replies with a hard coded response.
  virtual void DispatchEventToExtension(
      const std::string& extension_id,
      scoped_ptr<extensions::Event> event) override {
    ASSERT_TRUE(file_system_);
    std::string file_system_id;
    const base::DictionaryValue* dictionary_value = NULL;
    ASSERT_TRUE(event->event_args->GetDictionary(0, &dictionary_value));
    EXPECT_TRUE(dictionary_value->GetString("fileSystemId", &file_system_id));
    EXPECT_EQ(kFileSystemId, file_system_id);
    int request_id = -1;
    EXPECT_TRUE(dictionary_value->GetInteger("requestId", &request_id));
    EXPECT_TRUE(event->event_name ==
                    extensions::api::file_system_provider::
                        OnObserveDirectoryRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnUnobserveEntryRequested::kEventName);

    if (reply_result_ == base::File::FILE_OK) {
      base::ListValue value_as_list;
      value_as_list.Set(0, new base::StringValue(kFileSystemId));
      value_as_list.Set(1, new base::FundamentalValue(request_id));
      value_as_list.Set(2, new base::FundamentalValue(0) /* execution_time */);

      using extensions::api::file_system_provider_internal::
          OperationRequestedSuccess::Params;
      scoped_ptr<Params> params(Params::Create(value_as_list));
      ASSERT_TRUE(params.get());
      file_system_->GetRequestManager()->FulfillRequest(
          request_id,
          RequestValue::CreateForOperationSuccess(params.Pass()),
          false /* has_more */);
    } else {
      file_system_->GetRequestManager()->RejectRequest(
          request_id, make_scoped_ptr(new RequestValue()), reply_result_);
    }
  }

  void set_reply_result(base::File::Error result) { reply_result_ = result; }

 private:
  ProvidedFileSystemInterface* const file_system_;  // Not owned.
  base::File::Error reply_result_;
  DISALLOW_COPY_AND_ASSIGN(FakeEventRouter);
};

// Observes the tested file system.
class Observer : public ProvidedFileSystemObserver {
 public:
  class ChangeEvent {
   public:
    ChangeEvent(ProvidedFileSystemObserver::ChangeType change_type,
                const ProvidedFileSystemObserver::Changes& changes)
        : change_type_(change_type), changes_(changes) {}
    virtual ~ChangeEvent() {}

    ProvidedFileSystemObserver::ChangeType change_type() const {
      return change_type_;
    }
    const ProvidedFileSystemObserver::Changes& changes() const {
      return changes_;
    }

   private:
    const ProvidedFileSystemObserver::ChangeType change_type_;
    const ProvidedFileSystemObserver::Changes changes_;

    DISALLOW_COPY_AND_ASSIGN(ChangeEvent);
  };

  Observer() : list_changed_counter_(0), tag_updated_counter_(0) {}

  // ProvidedFileSystemInterfaceObserver overrides.
  virtual void OnObservedEntryChanged(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry,
      ProvidedFileSystemObserver::ChangeType change_type,
      const ProvidedFileSystemObserver::Changes& changes,
      const base::Closure& callback) override {
    EXPECT_EQ(kFileSystemId, file_system_info.file_system_id());
    change_events_.push_back(new ChangeEvent(change_type, changes));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  }

  virtual void OnObservedEntryTagUpdated(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry) override {
    EXPECT_EQ(kFileSystemId, file_system_info.file_system_id());
    ++tag_updated_counter_;
  }

  virtual void OnObservedEntryListChanged(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntries& observed_entries) override {
    EXPECT_EQ(kFileSystemId, file_system_info.file_system_id());
    ++list_changed_counter_;
  }

  int list_changed_counter() const { return list_changed_counter_; }
  const ScopedVector<ChangeEvent>& change_events() const {
    return change_events_;
  }
  int tag_updated_counter() const { return tag_updated_counter_; }

 private:
  ScopedVector<ChangeEvent> change_events_;
  int list_changed_counter_;
  int tag_updated_counter_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// Stub notification manager, which works in unit tests.
class StubNotificationManager : public NotificationManagerInterface {
 public:
  StubNotificationManager() {}
  virtual ~StubNotificationManager() {}

  // NotificationManagerInterface overrides.
  virtual void ShowUnresponsiveNotification(
      int id,
      const NotificationCallback& callback) override {}
  virtual void HideUnresponsiveNotification(int id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNotificationManager);
};

typedef std::vector<base::File::Error> Log;

// Writes a |result| to the |log| vector.
void LogStatus(Log* log, base::File::Error result) {
  log->push_back(result);
}

}  // namespace

class FileSystemProviderProvidedFileSystemTest : public testing::Test {
 protected:
  FileSystemProviderProvidedFileSystemTest() {}
  virtual ~FileSystemProviderProvidedFileSystemTest() {}

  virtual void SetUp() override {
    profile_.reset(new TestingProfile);
    const base::FilePath mount_path =
        util::GetMountPath(profile_.get(), kExtensionId, kFileSystemId);
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    file_system_info_.reset(
        new ProvidedFileSystemInfo(kExtensionId, mount_options, mount_path));
    provided_file_system_.reset(
        new ProvidedFileSystem(profile_.get(), *file_system_info_.get()));
    event_router_.reset(
        new FakeEventRouter(profile_.get(), provided_file_system_.get()));
    event_router_->AddEventListener(extensions::api::file_system_provider::
                                        OnObserveDirectoryRequested::kEventName,
                                    NULL,
                                    kExtensionId);
    event_router_->AddEventListener(extensions::api::file_system_provider::
                                        OnUnobserveEntryRequested::kEventName,
                                    NULL,
                                    kExtensionId);
    provided_file_system_->SetEventRouterForTesting(event_router_.get());
    provided_file_system_->SetNotificationManagerForTesting(
        make_scoped_ptr(new StubNotificationManager));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<FakeEventRouter> event_router_;
  scoped_ptr<ProvidedFileSystemInfo> file_system_info_;
  scoped_ptr<ProvidedFileSystem> provided_file_system_;
};

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater) {
  Log log;
  base::Closure firstCallback;
  base::Closure secondCallback;

  {
    // Auto updater is ref counted, and bound to all callbacks.
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(
        base::Bind(&LogStatus, base::Unretained(&log), base::File::FILE_OK)));

    firstCallback = auto_updater->CreateCallback();
    secondCallback = auto_updater->CreateCallback();
  }

  // Getting out of scope, should not invoke updating if there are pending
  // callbacks.
  EXPECT_EQ(0u, log.size());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, firstCallback);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, log.size());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, secondCallback);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater_NoCallbacks) {
  Log log;
  {
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(
        base::Bind(&LogStatus, base::Unretained(&log), base::File::FILE_OK)));
  }
  EXPECT_EQ(1u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater_CallbackIgnored) {
  Log log;
  {
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(
        base::Bind(&LogStatus, base::Unretained(&log), base::File::FILE_OK)));
    base::Closure callback = auto_updater->CreateCallback();
    // The callback gets out of scope, so the ref counted auto updater instance
    // gets deleted. Still, updating shouldn't be invoked, since the callback
    // wasn't executed.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, ObserveDirectory_NotFound) {
  Log log;
  Observer observer;

  provided_file_system_->AddObserver(&observer);

  // First, set the extension response to an error.
  event_router_->set_reply_result(base::File::FILE_ERROR_NOT_FOUND);

  provided_file_system_->ObserveDirectory(
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      false /* recursive */,
      base::Bind(&LogStatus, base::Unretained(&log)));
  base::RunLoop().RunUntilIdle();

  // The directory should not become observed because of an error.
  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);

  ObservedEntries* const observed_entries =
      provided_file_system_->GetObservedEntries();
  EXPECT_EQ(0u, observed_entries->size());

  // The observer should not be called.
  EXPECT_EQ(0, observer.list_changed_counter());
  EXPECT_EQ(0, observer.tag_updated_counter());

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, ObserveDirectory) {
  Log log;
  Observer observer;

  provided_file_system_->AddObserver(&observer);

  provided_file_system_->ObserveDirectory(
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      false /* recursive */,
      base::Bind(&LogStatus, base::Unretained(&log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0]);
  EXPECT_EQ(1, observer.list_changed_counter());
  EXPECT_EQ(0, observer.tag_updated_counter());

  ObservedEntries* const observed_entries =
      provided_file_system_->GetObservedEntries();
  ASSERT_EQ(1u, observed_entries->size());
  const ObservedEntry& observed_entry = observed_entries->begin()->second;
  EXPECT_EQ(kDirectoryPath, observed_entry.entry_path.value());
  EXPECT_FALSE(observed_entry.recursive);
  EXPECT_EQ("", observed_entry.last_tag);

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, ObserveDirectory_Exists) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  {
    // First observe a directory not recursively.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  {
    // Create another non-recursive observer. That should fail.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_EXISTS, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());  // No changes on the list.
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  {
    // Create another observer on the same path, but a recursive one. That
    // should succeed.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        true /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    const auto& observed_entry_it = observed_entries->find(
        ObservedEntryKey(base::FilePath(FILE_PATH_LITERAL(kDirectoryPath)),
                         true /* recursive */));
    ASSERT_NE(observed_entries->end(), observed_entry_it);

    EXPECT_EQ(kDirectoryPath, observed_entry_it->second.entry_path.value());
    EXPECT_TRUE(observed_entry_it->second.recursive);
    EXPECT_EQ("", observed_entry_it->second.last_tag);
  }

  {
    // Lastly, create another recursive observer. That should fail, too.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        true /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_EXISTS, log[0]);
    EXPECT_EQ(2, observer.list_changed_counter());  // No changes on the list.
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, UnobserveEntry) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  {
    // First, confirm that unobserving an entry which is not observed, results
    // in an error.
    Log log;
    provided_file_system_->UnobserveEntry(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);
    EXPECT_EQ(0, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  {
    // Observe a directory not recursively.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(1u, observed_entries->size());
  }

  {
    // Unobserve it gracefully.
    Log log;
    provided_file_system_->UnobserveEntry(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(0u, observed_entries->size());
  }

  {
    // Confirm that it's possible to observe it again.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(3, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(1u, observed_entries->size());
  }

  {
    // Finally, unobserve it, but with an error from extension. That should
    // result
    // in a removed observer, anyway.
    event_router_->set_reply_result(base::File::FILE_ERROR_FAILED);

    Log log;
    provided_file_system_->UnobserveEntry(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_FAILED, log[0]);
    EXPECT_EQ(4, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(0u, observed_entries->size());
  }

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, Notify) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  {
    // Observe a directory.
    Log log;
    provided_file_system_->ObserveDirectory(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(1u, observed_entries->size());
    provided_file_system_->GetObservedEntries();
    EXPECT_EQ("", observed_entries->begin()->second.last_tag);
  }

  {
    // Notify about a change.
    const ProvidedFileSystemObserver::ChangeType change_type =
        ProvidedFileSystemObserver::CHANGED;
    const std::string tag = "hello-world";
    EXPECT_TRUE(provided_file_system_->Notify(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        change_type,
        make_scoped_ptr(new ProvidedFileSystemObserver::Changes),
        tag));

    // Verify the observer event.
    ASSERT_EQ(1u, observer.change_events().size());
    const Observer::ChangeEvent* const change_event =
        observer.change_events()[0];
    EXPECT_EQ(change_type, change_event->change_type());
    EXPECT_EQ(0u, change_event->changes().size());

    // The tag should not be updated in advance, before all observers handle
    // the notification.
    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(1u, observed_entries->size());
    provided_file_system_->GetObservedEntries();
    EXPECT_EQ("", observed_entries->begin()->second.last_tag);

    // Wait until all observers finish handling the notification.
    base::RunLoop().RunUntilIdle();

    // Confirm, that the entry is still being observed, and that the tag is
    // updated.
    ASSERT_EQ(1u, observed_entries->size());
    EXPECT_EQ(tag, observed_entries->begin()->second.last_tag);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(1, observer.tag_updated_counter());
  }

  {
    // Notify about deleting of the observed entry.
    const ProvidedFileSystemObserver::ChangeType change_type =
        ProvidedFileSystemObserver::DELETED;
    const ProvidedFileSystemObserver::Changes changes;
    const std::string tag = "chocolate-disco";
    EXPECT_TRUE(provided_file_system_->Notify(
        base::FilePath::FromUTF8Unsafe(kDirectoryPath),
        false /* recursive */,
        change_type,
        make_scoped_ptr(new ProvidedFileSystemObserver::Changes),
        tag));
    base::RunLoop().RunUntilIdle();

    // Verify the observer event.
    ASSERT_EQ(2u, observer.change_events().size());
    const Observer::ChangeEvent* const change_event =
        observer.change_events()[1];
    EXPECT_EQ(change_type, change_event->change_type());
    EXPECT_EQ(0u, change_event->changes().size());
  }

  // Confirm, that the entry is not observed anymore.
  {
    ObservedEntries* const observed_entries =
        provided_file_system_->GetObservedEntries();
    EXPECT_EQ(0u, observed_entries->size());
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(2, observer.tag_updated_counter());
  }

  provided_file_system_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
