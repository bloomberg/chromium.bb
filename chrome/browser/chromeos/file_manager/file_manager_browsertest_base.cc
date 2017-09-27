// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/fileapi/external_mount_points.h"

namespace file_manager {
namespace {

enum EntryType {
  FILE,
  DIRECTORY,
};

enum TargetVolume {
  LOCAL_VOLUME,
  DRIVE_VOLUME,
  USB_VOLUME,
};

enum SharedOption {
  NONE,
  SHARED,
};

// Obtains file manager test data directory.
base::FilePath GetTestFilePath(const std::string& relative_path) {
  base::FilePath path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &path))
    return base::FilePath();
  path = path.AppendASCII("chrome")
             .AppendASCII("test")
             .AppendASCII("data")
             .AppendASCII("chromeos")
             .AppendASCII("file_manager")
             .Append(base::FilePath::FromUTF8Unsafe(relative_path));
  return path;
}

// Maps the given string to EntryType. Returns true on success.
bool MapStringToEntryType(const base::StringPiece& value, EntryType* output) {
  if (value == "file")
    *output = FILE;
  else if (value == "directory")
    *output = DIRECTORY;
  else
    return false;
  return true;
}

// Maps the given string to SharedOption. Returns true on success.
bool MapStringToSharedOption(const base::StringPiece& value,
                             SharedOption* output) {
  if (value == "shared")
    *output = SHARED;
  else if (value == "none")
    *output = NONE;
  else
    return false;
  return true;
}

// Maps the given string to TargetVolume. Returns true on success.
bool MapStringToTargetVolume(const base::StringPiece& value,
                             TargetVolume* output) {
  if (value == "drive")
    *output = DRIVE_VOLUME;
  else if (value == "local")
    *output = LOCAL_VOLUME;
  else if (value == "usb")
    *output = USB_VOLUME;
  else
    return false;
  return true;
}

// Maps the given string to base::Time. Returns true on success.
bool MapStringToTime(const base::StringPiece& value, base::Time* time) {
  return base::Time::FromString(value.as_string().c_str(), time);
}

// Test data of file or directory.
struct TestEntryInfo {
  TestEntryInfo() : type(FILE), shared_option(NONE) {}

  TestEntryInfo(EntryType type,
                const std::string& source_file_name,
                const std::string& target_path,
                const std::string& mime_type,
                SharedOption shared_option,
                const base::Time& last_modified_time)
      : type(type),
        source_file_name(source_file_name),
        target_path(target_path),
        mime_type(mime_type),
        shared_option(shared_option),
        last_modified_time(last_modified_time) {}

  EntryType type;
  std::string source_file_name;  // Source file name to be used as a prototype.
  std::string target_path;       // Target file or directory path.
  std::string mime_type;
  SharedOption shared_option;
  base::Time last_modified_time;

  // Registers the member information to the given converter.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TestEntryInfo>* converter);
};

// static
void TestEntryInfo::RegisterJSONConverter(
    base::JSONValueConverter<TestEntryInfo>* converter) {
  converter->RegisterCustomField("type", &TestEntryInfo::type,
                                 &MapStringToEntryType);
  converter->RegisterStringField("sourceFileName",
                                 &TestEntryInfo::source_file_name);
  converter->RegisterStringField("targetPath", &TestEntryInfo::target_path);
  converter->RegisterStringField("mimeType", &TestEntryInfo::mime_type);
  converter->RegisterCustomField("sharedOption", &TestEntryInfo::shared_option,
                                 &MapStringToSharedOption);
  converter->RegisterCustomField(
      "lastModifiedTime", &TestEntryInfo::last_modified_time, &MapStringToTime);
}

// Message from JavaScript to add entries.
struct AddEntriesMessage {
  // Target volume to be added the |entries|.
  TargetVolume volume;

  // Entries to be added.
  std::vector<std::unique_ptr<TestEntryInfo>> entries;

  // Registers the member information to the given converter.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AddEntriesMessage>* converter);
};

// static
void AddEntriesMessage::RegisterJSONConverter(
    base::JSONValueConverter<AddEntriesMessage>* converter) {
  converter->RegisterCustomField("volume", &AddEntriesMessage::volume,
                                 &MapStringToTargetVolume);
  converter->RegisterRepeatedMessage<TestEntryInfo>(
      "entries", &AddEntriesMessage::entries);
}

// Test volume.
class TestVolume {
 protected:
  explicit TestVolume(const std::string& name) : name_(name) {}
  virtual ~TestVolume() {}

  bool CreateRootDirectory(const Profile* profile) {
    if (root_initialized_)
      return true;

    root_initialized_ = root_.Set(profile->GetPath().Append(name_));
    return root_initialized_;
  }

  const std::string& name() const { return name_; }
  const base::FilePath& root_path() const { return root_.GetPath(); }

 private:
  std::string name_;
  base::ScopedTempDir root_;
  bool root_initialized_ = false;
};

// Listener to obtain the test relative messages synchronously.
class FileManagerTestListener : public content::NotificationObserver {
 public:
  struct Message {
    int type;
    std::string message;
    scoped_refptr<extensions::TestSendMessageFunction> function;
  };

  FileManagerTestListener() {
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_FAILED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  Message GetNextMessage() {
    if (messages_.empty())
      content::RunMessageLoop();
    const Message entry = messages_.front();
    messages_.pop_front();
    return entry;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    Message entry;
    entry.type = type;
    entry.message = type != extensions::NOTIFICATION_EXTENSION_TEST_PASSED
                        ? *content::Details<std::string>(details).ptr()
                        : std::string();
    if (type == extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE) {
      entry.function =
          content::Source<extensions::TestSendMessageFunction>(source).ptr();
      *content::Details<std::pair<std::string, bool*>>(details).ptr()->second =
          true;
    }
    messages_.push_back(entry);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  base::circular_deque<Message> messages_;
  content::NotificationRegistrar registrar_;
};

}  // anonymous namespace

// The local volume class for test.
// This class provides the operations for a test volume that simulates local
// drive.
class LocalTestVolume : public TestVolume {
 public:
  explicit LocalTestVolume(const std::string& name) : TestVolume(name) {}
  ~LocalTestVolume() override {}

  // Adds this volume to the file system as a local volume. Returns true on
  // success.
  virtual bool Mount(Profile* profile) = 0;

  void CreateEntry(const TestEntryInfo& entry) {
    const base::FilePath target_path =
        root_path().AppendASCII(entry.target_path);

    entries_.insert(std::make_pair(target_path, entry));
    switch (entry.type) {
      case FILE: {
        const base::FilePath source_path =
            GetTestFilePath(entry.source_file_name);
        ASSERT_TRUE(base::CopyFile(source_path, target_path))
            << "Copy from " << source_path.value() << " to "
            << target_path.value() << " failed.";
        break;
      }
      case DIRECTORY:
        ASSERT_TRUE(base::CreateDirectory(target_path))
            << "Failed to create a directory: " << target_path.value();
        break;
    }
    ASSERT_TRUE(UpdateModifiedTime(entry));
  }

 private:
  // Updates ModifiedTime of the entry and its parents by referring
  // TestEntryInfo. Returns true on success.
  bool UpdateModifiedTime(const TestEntryInfo& entry) {
    const base::FilePath path = root_path().AppendASCII(entry.target_path);
    if (!base::TouchFile(path, entry.last_modified_time,
                         entry.last_modified_time))
      return false;

    // Update the modified time of parent directories because it may be also
    // affected by the update of child items.
    if (path.DirName() != root_path()) {
      const std::map<base::FilePath, const TestEntryInfo>::iterator it =
          entries_.find(path.DirName());
      if (it == entries_.end())
        return false;
      return UpdateModifiedTime(it->second);
    }
    return true;
  }

  std::map<base::FilePath, const TestEntryInfo> entries_;
};

class DownloadsTestVolume : public LocalTestVolume {
 public:
  DownloadsTestVolume() : LocalTestVolume("Downloads") {}
  ~DownloadsTestVolume() override {}

  bool Mount(Profile* profile) override {
    return CreateRootDirectory(profile) &&
           VolumeManager::Get(profile)
               ->RegisterDownloadsDirectoryForTesting(root_path());
  }
};

// Test volume for mimicing a specified type of volumes by a local folder.
class FakeTestVolume : public LocalTestVolume {
 public:
  FakeTestVolume(const std::string& name,
                 VolumeType volume_type,
                 chromeos::DeviceType device_type)
      : LocalTestVolume(name),
        volume_type_(volume_type),
        device_type_(device_type) {}
  ~FakeTestVolume() override {}

  // Simple test entries used for testing, e.g., read-only volumes.
  bool PrepareTestEntries(Profile* profile) {
    if (!CreateRootDirectory(profile))
      return false;
    // Must be in sync with BASIC_FAKE_ENTRY_SET in the JS test code.
    CreateEntry(TestEntryInfo(FILE, "text.txt", "hello.txt", "text/plain", NONE,
                              base::Time::Now()));
    CreateEntry(TestEntryInfo(DIRECTORY, std::string(), "A", std::string(),
                              NONE, base::Time::Now()));
    return true;
  }

  bool Mount(Profile* profile) override {
    if (!CreateRootDirectory(profile))
      return false;
    storage::ExternalMountPoints* const mount_points =
        storage::ExternalMountPoints::GetSystemInstance();

    // First revoke the existing mount point (if any).
    mount_points->RevokeFileSystem(name());
    const bool result = mount_points->RegisterFileSystem(
        name(), storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), root_path());
    if (!result)
      return false;

    VolumeManager::Get(profile)->AddVolumeForTesting(
        root_path(), volume_type_, device_type_, false /* read_only */);
    return true;
  }

 private:
  const VolumeType volume_type_;
  const chromeos::DeviceType device_type_;
};

// The drive volume class for test.
// This class provides the operations for a test volume that simulates Google
// drive.
class DriveTestVolume : public TestVolume {
 public:
  DriveTestVolume() : TestVolume("drive"), integration_service_(NULL) {}
  ~DriveTestVolume() override {}

  void CreateEntry(const TestEntryInfo& entry) {
    const base::FilePath path =
        base::FilePath::FromUTF8Unsafe(entry.target_path);
    const std::string target_name = path.BaseName().AsUTF8Unsafe();

    // Obtain the parent entry.
    drive::FileError error = drive::FILE_ERROR_OK;
    std::unique_ptr<drive::ResourceEntry> parent_entry(
        new drive::ResourceEntry);
    integration_service_->file_system()->GetResourceEntry(
        drive::util::GetDriveMyDriveRootPath().Append(path).DirName(),
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &parent_entry));
    content::RunAllTasksUntilIdle();
    ASSERT_EQ(drive::FILE_ERROR_OK, error);
    ASSERT_TRUE(parent_entry);

    switch (entry.type) {
      case FILE:
        CreateFile(entry.source_file_name, parent_entry->resource_id(),
                   target_name, entry.mime_type, entry.shared_option == SHARED,
                   entry.last_modified_time);
        break;
      case DIRECTORY:
        CreateDirectory(parent_entry->resource_id(), target_name,
                        entry.last_modified_time);
        break;
    }
  }

  // Creates an empty directory with the given |name| and |modification_time|.
  void CreateDirectory(const std::string& parent_id,
                       const std::string& target_name,
                       const base::Time& modification_time) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    fake_drive_service_->AddNewDirectory(
        parent_id, target_name, drive::AddNewDirectoryOptions(),
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);

    fake_drive_service_->SetLastModifiedTime(
        entry->file_id(), modification_time,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(error == google_apis::HTTP_SUCCESS);
    ASSERT_TRUE(entry);
    CheckForUpdates();
  }

  // Creates a test file with the given spec.
  // Serves |test_file_name| file. Pass an empty string for an empty file.
  void CreateFile(const std::string& source_file_name,
                  const std::string& parent_id,
                  const std::string& target_name,
                  const std::string& mime_type,
                  bool shared_with_me,
                  const base::Time& modification_time) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;

    std::string content_data;
    if (!source_file_name.empty()) {
      base::FilePath source_file_path = GetTestFilePath(source_file_name);
      ASSERT_TRUE(base::ReadFileToString(source_file_path, &content_data));
    }

    std::unique_ptr<google_apis::FileResource> entry;
    fake_drive_service_->AddNewFile(
        mime_type, content_data, parent_id, target_name, shared_with_me,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);

    fake_drive_service_->SetLastModifiedTime(
        entry->file_id(), modification_time,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
    ASSERT_TRUE(entry);

    CheckForUpdates();
  }

  // Notifies FileSystem that the contents in FakeDriveService are
  // changed, hence the new contents should be fetched.
  void CheckForUpdates() {
    if (integration_service_ && integration_service_->file_system()) {
      integration_service_->file_system()->CheckForUpdates();
    }
  }

  // Sets the url base for the test server to be used to generate share urls
  // on the files and directories.
  void ConfigureShareUrlBase(const GURL& share_url_base) {
    fake_drive_service_->set_share_url_base(share_url_base);
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    profile_ = profile;
    fake_drive_service_ = new drive::FakeDriveService;
    fake_drive_service_->LoadAppListForDriveApi("drive/applist.json");

    if (!CreateRootDirectory(profile))
      return NULL;
    integration_service_ = new drive::DriveIntegrationService(
        profile, NULL, fake_drive_service_, std::string(), root_path(), NULL);
    return integration_service_;
  }

 private:
  Profile* profile_;
  drive::FakeDriveService* fake_drive_service_;
  drive::DriveIntegrationService* integration_service_;
};

FileManagerBrowserTestBase::FileManagerBrowserTestBase() {
}

FileManagerBrowserTestBase::~FileManagerBrowserTestBase() {
}

void FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

  local_volume_.reset(new DownloadsTestVolume);
  if (GetGuestModeParam() != IN_GUEST_MODE) {
    create_drive_integration_service_ =
        base::Bind(&FileManagerBrowserTestBase::CreateDriveIntegrationService,
                   base::Unretained(this));
    service_factory_for_test_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }
}

void FileManagerBrowserTestBase::SetUp() {
  net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
  ExtensionApiTest::SetUp();
}

void FileManagerBrowserTestBase::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();
  ASSERT_TRUE(local_volume_->Mount(profile()));

  // The file manager component app should have been added for loading into the
  // user profile, but not into the sign-in profile.
  ASSERT_TRUE(extensions::ExtensionSystem::Get(profile())
                  ->extension_service()
                  ->component_loader()
                  ->Exists(kFileManagerAppId));
  ASSERT_FALSE(extensions::ExtensionSystem::Get(
                   chromeos::ProfileHelper::GetSigninProfile())
                   ->extension_service()
                   ->component_loader()
                   ->Exists(kFileManagerAppId));

  if (GetGuestModeParam() != IN_GUEST_MODE) {
    // Install the web server to serve the mocked share dialog.
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL share_url_base(embedded_test_server()->GetURL(
        "/chromeos/file_manager/share_dialog_mock/index.html"));
    drive_volume_ = drive_volumes_[profile()->GetOriginalProfile()];
    drive_volume_->ConfigureShareUrlBase(share_url_base);
    test_util::WaitUntilDriveMountPointIsAdded(profile());
  }
}

void FileManagerBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (GetGuestModeParam() == IN_GUEST_MODE) {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitchNative(chromeos::switches::kLoginUser, "");
    command_line->AppendSwitch(switches::kIncognito);
  }
  if (GetGuestModeParam() == IN_INCOGNITO) {
    command_line->AppendSwitch(switches::kIncognito);
  }
  ExtensionApiTest::SetUpCommandLine(command_line);
}

void FileManagerBrowserTestBase::InstallExtension(const base::FilePath& path,
                                                  const char* manifest_name) {
  base::FilePath root_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &root_path));

  // Launch the extension.
  const base::FilePath absolute_path = root_path.Append(path);
  const extensions::Extension* const extension =
      LoadExtensionAsComponentWithManifest(absolute_path, manifest_name);
  ASSERT_TRUE(extension);
}

void FileManagerBrowserTestBase::StartTest() {
  InstallExtension(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/integration_tests")),
      GetTestManifestName());
  RunTestMessageLoop();
}

void FileManagerBrowserTestBase::RunTestMessageLoop() {
  // Handle the messages from JavaScript.
  // The while loop is break when the test is passed or failed.
  FileManagerTestListener listener;
  while (true) {
    FileManagerTestListener::Message entry = listener.GetNextMessage();
    if (entry.type == extensions::NOTIFICATION_EXTENSION_TEST_PASSED) {
      // Test succeed.
      break;
    } else if (entry.type == extensions::NOTIFICATION_EXTENSION_TEST_FAILED) {
      // Test failed.
      ADD_FAILURE() << entry.message;
      break;
    }

    // Parse the message value as JSON.
    const std::unique_ptr<const base::Value> value =
        base::JSONReader::Read(entry.message);

    // If the message is not the expected format, just ignore it.
    const base::DictionaryValue* message_dictionary = NULL;
    std::string name;
    if (!value || !value->GetAsDictionary(&message_dictionary) ||
        !message_dictionary->GetString("name", &name)) {
      entry.function->Reply(std::string());
      continue;
    }

    std::string output;
    OnMessage(name, *message_dictionary, &output);
    if (HasFatalFailure())
      break;

    entry.function->Reply(output);
  }
}

void FileManagerBrowserTestBase::OnMessage(const std::string& name,
                                           const base::DictionaryValue& value,
                                           std::string* output) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (name == "getTestName") {
    // Pass the test case name.
    *output = GetTestCaseNameParam();
    return;
  }

  if (name == "getRootPaths") {
    // Pass the root paths.
    base::DictionaryValue res;
    res.SetString("downloads",
                  "/" + util::GetDownloadsMountPointName(profile()));
    res.SetString("drive", "/" +
                               drive::util::GetDriveMountPointPath(profile())
                                   .BaseName()
                                   .AsUTF8Unsafe() +
                               "/root");
    base::JSONWriter::Write(res, output);
    return;
  }

  if (name == "isInGuestMode") {
    // Obtain whether the test is in guest mode or not.
    *output = GetGuestModeParam() != NOT_IN_GUEST_MODE ? "true" : "false";
    return;
  }

  if (name == "getCwsWidgetContainerMockUrl") {
    // Obtain whether the test is in guest mode or not.
    const GURL url = embedded_test_server()->GetURL(
        "/chromeos/file_manager/cws_container_mock/index.html");
    std::string origin = url.GetOrigin().spec();

    // Removes trailing a slash.
    if (*origin.rbegin() == '/')
      origin.resize(origin.length() - 1);

    base::DictionaryValue res;
    res.SetString("url", url.spec());
    res.SetString("origin", origin);
    base::JSONWriter::Write(res, output);
    return;
  }

  if (name == "addEntries") {
    // Add entries to the specified volume.
    base::JSONValueConverter<AddEntriesMessage> add_entries_message_converter;
    AddEntriesMessage message;
    ASSERT_TRUE(add_entries_message_converter.Convert(value, &message));

    for (size_t i = 0; i < message.entries.size(); ++i) {
      switch (message.volume) {
        case LOCAL_VOLUME:
          local_volume_->CreateEntry(*message.entries[i]);
          break;
        case DRIVE_VOLUME:
          if (drive_volume_.get())
            drive_volume_->CreateEntry(*message.entries[i]);
          break;
        case USB_VOLUME:
          if (usb_volume_)
            usb_volume_->CreateEntry(*message.entries[i]);
          break;
        default:
          NOTREACHED();
          break;
      }
    }

    return;
  }

  if (name == "mountFakeUsb") {
    usb_volume_.reset(new FakeTestVolume("fake-usb",
                                         VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
                                         chromeos::DEVICE_TYPE_USB));
    usb_volume_->Mount(profile());
    return;
  }

  if (name == "mountFakeMtp") {
    mtp_volume_.reset(new FakeTestVolume("fake-mtp", VOLUME_TYPE_MTP,
                                         chromeos::DEVICE_TYPE_UNKNOWN));
    ASSERT_TRUE(mtp_volume_->PrepareTestEntries(profile()));

    mtp_volume_->Mount(profile());
    return;
  }

  if (name == "useCellularNetwork") {
    net::NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChangeForTests(
        net::NetworkChangeNotifier::GetMaxBandwidthForConnectionSubtype(
            net::NetworkChangeNotifier::SUBTYPE_HSPA),
        net::NetworkChangeNotifier::CONNECTION_3G);
    return;
  }

  if (name == "clickNotificationButton") {
    std::string extension_id;
    std::string notification_id;
    int index;
    ASSERT_TRUE(value.GetString("extensionId", &extension_id));
    ASSERT_TRUE(value.GetString("notificationId", &notification_id));
    ASSERT_TRUE(value.GetInteger("index", &index));

    const std::string delegate_id = extension_id + "-" + notification_id;
    const Notification* notification =
        g_browser_process->notification_ui_manager()->FindById(delegate_id,
                                                               profile());
    ASSERT_TRUE(notification);

    notification->delegate()->ButtonClick(index);
    return;
  }

  if (name == "installProviderExtension") {
    std::string manifest;
    ASSERT_TRUE(value.GetString("manifest", &manifest));
    InstallExtension(base::FilePath(FILE_PATH_LITERAL(
                         "ui/file_manager/integration_tests/testing_provider")),
                     manifest.c_str());
    return;
  }

  FAIL() << "Unknown test message: " << name;
}

drive::DriveIntegrationService*
FileManagerBrowserTestBase::CreateDriveIntegrationService(Profile* profile) {
  drive_volumes_[profile->GetOriginalProfile()].reset(new DriveTestVolume());
  return drive_volumes_[profile->GetOriginalProfile()]
      ->CreateDriveIntegrationService(profile);
}

}  // namespace file_manager
