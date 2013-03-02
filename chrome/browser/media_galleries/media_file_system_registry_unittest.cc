// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry unit tests.

#include <algorithm>
#include <set>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_context.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "sync/api/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/browser/storage_monitor/test_portable_device_watcher_win.h"
#include "chrome/browser/storage_monitor/test_removable_device_notifications_window_win.h"
#include "chrome/browser/storage_monitor/test_volume_mount_watcher_win.h"
#include "chrome/common/chrome_switches.h"
#endif

namespace chrome {

// Not anonymous so it can be friends with MediaFileSystemRegistry.
class TestMediaFileSystemContext : public MediaFileSystemContext {
 public:
  struct FSInfo {
    FSInfo() {}
    FSInfo(const std::string& device_id, const base::FilePath& path,
           const std::string& fsid);

    bool operator<(const FSInfo& other) const;

    std::string device_id;
    base::FilePath path;
    std::string fsid;
  };

  explicit TestMediaFileSystemContext(MediaFileSystemRegistry* registry);
  virtual ~TestMediaFileSystemContext() {}

  // MediaFileSystemContext implementation.
  virtual std::string RegisterFileSystemForMassStorage(
      const std::string& device_id, const base::FilePath& path) OVERRIDE;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  virtual std::string RegisterFileSystemForMTPDevice(
      const std::string& device_id, const base::FilePath& path,
      scoped_refptr<ScopedMTPDeviceMapEntry>* entry) OVERRIDE;
#endif

  virtual void RevokeFileSystem(const std::string& fsid) OVERRIDE;

  virtual MediaFileSystemRegistry* GetMediaFileSystemRegistry() OVERRIDE;

  base::FilePath GetPathForId(const std::string& fsid) const;

 private:
  std::string AddFSEntry(const std::string& device_id,
                         const base::FilePath& path);

  MediaFileSystemRegistry* registry_;

  // A counter used to construct mock FSIDs.
  int fsid_;

  // The currently allocated mock file systems.
  std::map<std::string /*fsid*/, FSInfo> file_systems_by_id_;
};

TestMediaFileSystemContext::FSInfo::FSInfo(const std::string& device_id,
                                           const base::FilePath& path,
                                           const std::string& fsid)
    : device_id(device_id),
      path(path),
      fsid(fsid) {
}

bool TestMediaFileSystemContext::FSInfo::operator<(const FSInfo& other) const {
  if (device_id != other.device_id)
    return device_id < other.device_id;
  if (path.value() != other.path.value())
    return path.value() < other.path.value();
  return fsid < other.fsid;
}

TestMediaFileSystemContext::TestMediaFileSystemContext(
    MediaFileSystemRegistry* registry)
    : registry_(registry),
      fsid_(0) {
  registry_->file_system_context_.reset(this);
}

std::string TestMediaFileSystemContext::RegisterFileSystemForMassStorage(
    const std::string& device_id, const base::FilePath& path) {
  CHECK(MediaStorageUtil::IsMassStorageDevice(device_id));
  return AddFSEntry(device_id, path);
}

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
std::string TestMediaFileSystemContext::RegisterFileSystemForMTPDevice(
    const std::string& device_id, const base::FilePath& path,
    scoped_refptr<ScopedMTPDeviceMapEntry>* entry) {
  CHECK(!MediaStorageUtil::IsMassStorageDevice(device_id));
  DCHECK(entry);
  *entry = registry_->GetOrCreateScopedMTPDeviceMapEntry(path.value());
  return AddFSEntry(device_id, path);
}
#endif

void TestMediaFileSystemContext::RevokeFileSystem(const std::string& fsid) {
  if (!ContainsKey(file_systems_by_id_, fsid))
    return;
  EXPECT_EQ(1U, file_systems_by_id_.erase(fsid));
}

MediaFileSystemRegistry*
TestMediaFileSystemContext::GetMediaFileSystemRegistry() {
  return registry_;
}

base::FilePath TestMediaFileSystemContext::GetPathForId(
    const std::string& fsid) const {
  std::map<std::string /*fsid*/, FSInfo>::const_iterator it =
      file_systems_by_id_.find(fsid);
  if (it == file_systems_by_id_.end())
    return base::FilePath();
  return it->second.path;
}

std::string TestMediaFileSystemContext::AddFSEntry(const std::string& device_id,
                                                   const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(path.IsAbsolute());
  DCHECK(!path.ReferencesParent());

  std::string fsid = base::StringPrintf("FSID:%d", ++fsid_);
  FSInfo info(device_id, path, fsid);
  file_systems_by_id_[fsid] = info;
  return fsid;
}

namespace {

typedef std::map<MediaGalleryPrefId, MediaFileSystemInfo> FSInfoMap;

void GetGalleryInfoCallback(
    FSInfoMap* results,
    const std::vector<MediaFileSystemInfo>& file_systems) {
  for (size_t i = 0; i < file_systems.size(); ++i) {
    ASSERT_FALSE(ContainsKey(*results, file_systems[i].pref_id));
    (*results)[file_systems[i].pref_id] = file_systems[i];
  }
}

void CheckGalleryJSONName(const std::string& name, bool removable) {
  scoped_ptr<DictionaryValue> dict(static_cast<DictionaryValue*>(
      base::JSONReader::Read(name)));
  ASSERT_TRUE(dict);

  // Check deviceId.
  EXPECT_EQ(removable,
            dict->HasKey(MediaFileSystemRegistry::kDeviceIdKey)) << name;
  if (removable) {
    std::string device_id;
    EXPECT_TRUE(dict->GetString(MediaFileSystemRegistry::kDeviceIdKey,
                                &device_id)) << name;
    EXPECT_FALSE(device_id.empty()) << name;
  }

  // Check galleryId.
  EXPECT_TRUE(dict->HasKey(MediaFileSystemRegistry::kGalleryIdKey)) << name;

  // Check name.
  EXPECT_TRUE(dict->HasKey(MediaFileSystemRegistry::kNameKey)) << name;
  std::string gallery_name;
  EXPECT_TRUE(dict->GetString(MediaFileSystemRegistry::kNameKey,
                              &gallery_name)) << name;
  EXPECT_FALSE(gallery_name.empty()) << name;
}

void CheckGalleryInfo(const MediaFileSystemInfo& info,
                      TestMediaFileSystemContext* fs_context,
                      const base::FilePath& path,
                      bool removable,
                      bool media_device) {
  // TODO(vandebo) check the name (from path.LossyDisplayName) after JSON
  // removal.
  EXPECT_EQ(path, info.path);
  EXPECT_EQ(removable, info.removable);
  EXPECT_EQ(media_device, info.media_device);
  EXPECT_NE(0UL, info.pref_id);

  if (removable)
    EXPECT_NE(0UL, info.transient_device_id.size());
  else
    EXPECT_EQ(0UL, info.transient_device_id.size());

  base::FilePath fsid_path = fs_context->GetPathForId(info.fsid);
  EXPECT_EQ(path, fsid_path);
}

class MockProfileSharedRenderProcessHostFactory
    : public content::RenderProcessHostFactory {
 public:
  MockProfileSharedRenderProcessHostFactory() {}
  virtual ~MockProfileSharedRenderProcessHostFactory();

  // RPH created with this factory are owned by it.  If the RPH is destroyed
  // for testing purposes, it must be removed from the factory first.
  content::MockRenderProcessHost* ReleaseRPH(
      content::BrowserContext* browser_context);

  virtual content::RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context) const OVERRIDE;

 private:
  typedef std::map<content::BrowserContext*, content::MockRenderProcessHost*>
      ProfileRPHMap;
  mutable ProfileRPHMap rph_map_;

  DISALLOW_COPY_AND_ASSIGN(MockProfileSharedRenderProcessHostFactory);
};

class ProfileState {
 public:
  explicit ProfileState(
      MockProfileSharedRenderProcessHostFactory* rph_factory);
  ~ProfileState();

  MediaGalleriesPreferences* GetMediaGalleriesPrefs();

  void CheckGalleries(
      const std::string& test,
      const std::vector<MediaFileSystemInfo>& regular_extension_galleries,
      const std::vector<MediaFileSystemInfo>& all_extension_galleries);

  FSInfoMap GetGalleriesInfo(extensions::Extension* extension);

  extensions::Extension* all_permission_extension();
  extensions::Extension* regular_permission_extension();
  Profile* profile();

 private:
  void CompareResults(const std::string& test,
                      const std::vector<MediaFileSystemInfo>& expected,
                      const std::vector<MediaFileSystemInfo>& actual);

  int GetAndClearComparisonCount();

  int num_comparisons_;

  scoped_ptr<TestingProfile> profile_;

  scoped_refptr<extensions::Extension> all_permission_extension_;
  scoped_refptr<extensions::Extension> regular_permission_extension_;
  scoped_refptr<extensions::Extension> no_permissions_extension_;

  scoped_ptr<content::WebContents> single_web_contents_;
  scoped_ptr<content::WebContents> shared_web_contents1_;
  scoped_ptr<content::WebContents> shared_web_contents2_;

  // The RenderProcessHosts are freed when their respective WebContents /
  // RenderViewHosts go away.
  content::MockRenderProcessHost* single_rph_;
  content::MockRenderProcessHost* shared_rph_;

  DISALLOW_COPY_AND_ASSIGN(ProfileState);
};

}  // namespace

class MediaFileSystemRegistryTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFileSystemRegistryTest();
  virtual ~MediaFileSystemRegistryTest() {}

  void CreateProfileState(size_t profile_count);

  ProfileState* GetProfileState(size_t i);

  base::FilePath empty_dir() {
    return empty_dir_;
  }

  base::FilePath dcim_dir() {
    return dcim_dir_;
  }

  // Create a user added gallery based on the information passed and add it to
  // |profiles|. Returns the device id.
  std::string AddUserGallery(MediaStorageUtil::Type type,
                             const std::string& unique_id,
                             const base::FilePath& path);

  // Returns the device id.
  std::string AttachDevice(MediaStorageUtil::Type type,
                           const std::string& unique_id,
                           const base::FilePath& location);

  void DetachDevice(const std::string& device_id);

  void SetGalleryPermission(ProfileState* profile_state,
                            extensions::Extension* extension,
                            const std::string& device_id,
                            bool has_access);

  void AssertAllAutoAddedGalleries();

  void InitForGalleriesInfoTest(FSInfoMap* galleries_info);

  void CheckNewGalleryInfo(ProfileState* profile_state,
                           const FSInfoMap& galleries_info,
                           const base::FilePath& location,
                           bool removable,
                           bool media_device);

  std::vector<MediaFileSystemInfo> GetAutoAddedGalleries(
      ProfileState* profile_state);

  // TODO(gbillock): Rework these once windows-specific code is gone.
  void ProcessAttach(const std::string& id,
                     const string16& name,
                     const base::FilePath::StringType& location) {
    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        StorageMonitor::StorageInfo(id, name, location));
  }

  void ProcessDetach(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }

  MediaFileSystemRegistry* GetMediaFileSystemRegistry() {
    return test_file_system_context_->GetMediaFileSystemRegistry();
  }

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  // This makes sure that at least one default gallery exists on the file
  // system.
  EnsureMediaDirectoriesExists media_directories_;

  // Some test gallery directories.
  base::ScopedTempDir galleries_dir_;
  // An empty directory in |galleries_dir_|
  base::FilePath empty_dir_;
  // A directory in |galleries_dir_| with a DCIM directory in it.
  base::FilePath dcim_dir_;

  // MediaFileSystemRegistry owns this.
  TestMediaFileSystemContext* test_file_system_context_;

  // Needed for extension service & friends to work.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

// TODO(gbillock): Eliminate windows-specific code from this test.
#if defined(OS_WIN)
  scoped_ptr<test::TestRemovableDeviceNotificationsWindowWin> window_;
#else
  chrome::test::TestStorageMonitor monitor_;
#endif

  MockProfileSharedRenderProcessHostFactory rph_factory_;

  ScopedVector<ProfileState> profile_states_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistryTest);
};

namespace {

bool MediaFileSystemInfoComparator(const MediaFileSystemInfo& a,
                                   const MediaFileSystemInfo& b) {
  CHECK_NE(a.name, b.name);  // Name must be unique.
  return a.name < b.name;
}

///////////////////////////////////////////////
// MockProfileSharedRenderProcessHostFactory //
///////////////////////////////////////////////

MockProfileSharedRenderProcessHostFactory::
    ~MockProfileSharedRenderProcessHostFactory() {
  STLDeleteValues(&rph_map_);
}

content::MockRenderProcessHost*
MockProfileSharedRenderProcessHostFactory::ReleaseRPH(
    content::BrowserContext* browser_context) {
  ProfileRPHMap::iterator existing = rph_map_.find(browser_context);
  if (existing == rph_map_.end())
    return NULL;
  content::MockRenderProcessHost* result = existing->second;
  rph_map_.erase(existing);
  return result;
}

content::RenderProcessHost*
MockProfileSharedRenderProcessHostFactory::CreateRenderProcessHost(
    content::BrowserContext* browser_context) const {
  ProfileRPHMap::const_iterator existing = rph_map_.find(browser_context);
  if (existing != rph_map_.end())
    return existing->second;
  rph_map_[browser_context] =
      new content::MockRenderProcessHost(browser_context);
  return rph_map_[browser_context];
}

//////////////////
// ProfileState //
//////////////////

ProfileState::ProfileState(
    MockProfileSharedRenderProcessHostFactory* rph_factory)
    : num_comparisons_(0),
      profile_(new TestingProfile()) {
  extensions::TestExtensionSystem* extension_system(
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile_.get())));
  extension_system->CreateExtensionService(
      CommandLine::ForCurrentProcess(), base::FilePath(), false);

  std::vector<std::string> all_permissions;
  all_permissions.push_back("allAutoDetected");
  all_permissions.push_back("read");
  std::vector<std::string> read_permissions;
  read_permissions.push_back("read");

  all_permission_extension_ =
      AddMediaGalleriesApp("all", all_permissions, profile_.get());
  regular_permission_extension_ =
      AddMediaGalleriesApp("regular", read_permissions, profile_.get());
  no_permissions_extension_ =
      AddMediaGalleriesApp("no", read_permissions, profile_.get());

  single_web_contents_.reset(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  single_rph_ = rph_factory->ReleaseRPH(profile_.get());

  shared_web_contents1_.reset(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  shared_web_contents2_.reset(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  shared_rph_ = rph_factory->ReleaseRPH(profile_.get());
}

ProfileState::~ProfileState() {
  // TestExtensionSystem uses DeleteSoon, so we need to delete the profiles
  // and then run the message queue to clean up.  But first we have to
  // delete everything that references the profile.
  single_web_contents_.reset();
  shared_web_contents1_.reset();
  shared_web_contents2_.reset();
  profile_.reset();

  MessageLoop::current()->RunUntilIdle();
}

MediaGalleriesPreferences* ProfileState::GetMediaGalleriesPrefs() {
  return MediaGalleriesPreferencesFactory::GetForProfile(profile_.get());
}

void ProfileState::CheckGalleries(
    const std::string& test,
    const std::vector<MediaFileSystemInfo>& regular_extension_galleries,
    const std::vector<MediaFileSystemInfo>& all_extension_galleries) {
  content::RenderViewHost* rvh = single_web_contents_->GetRenderViewHost();
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();

  // No Media Galleries permissions.
  std::vector<MediaFileSystemInfo> empty_expectation;
  registry->GetMediaFileSystemsForExtension(
      rvh, no_permissions_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 StringPrintf("%s (no permission)", test.c_str()),
                 base::ConstRef(empty_expectation)));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());

  // Read permission only.
  registry->GetMediaFileSystemsForExtension(
      rvh, regular_permission_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 StringPrintf("%s (regular permission)", test.c_str()),
                 base::ConstRef(regular_extension_galleries)));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());

  // All galleries permission.
  registry->GetMediaFileSystemsForExtension(
      rvh, all_permission_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 StringPrintf("%s (all permission)", test.c_str()),
                 base::ConstRef(all_extension_galleries)));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());
}

FSInfoMap ProfileState::GetGalleriesInfo(extensions::Extension* extension) {
  content::RenderViewHost* rvh = single_web_contents_->GetRenderViewHost();
  FSInfoMap results;
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  registry->GetMediaFileSystemsForExtension(
      rvh, extension,
      base::Bind(&GetGalleryInfoCallback, base::Unretained(&results)));
  MessageLoop::current()->RunUntilIdle();
  return results;
}

extensions::Extension* ProfileState::all_permission_extension() {
  return all_permission_extension_.get();
}

extensions::Extension* ProfileState::regular_permission_extension() {
  return regular_permission_extension_.get();
}

Profile* ProfileState::profile() {
  return profile_.get();
}

void ProfileState::CompareResults(
    const std::string& test,
    const std::vector<MediaFileSystemInfo>& expected,
    const std::vector<MediaFileSystemInfo>& actual) {
  // Order isn't important, so sort the results.  Assume that expected
  // is already sorted.
  std::vector<MediaFileSystemInfo> sorted(actual);
  std::sort(sorted.begin(), sorted.end(), MediaFileSystemInfoComparator);

  num_comparisons_++;
  ASSERT_EQ(expected.size(), actual.size()) << test;
  for (size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
    EXPECT_EQ(expected[i].path, actual[i].path) << test;
    EXPECT_FALSE(actual[i].fsid.empty()) << test;
    if (!expected[i].fsid.empty())
      EXPECT_EQ(expected[i].fsid, actual[i].fsid) << test;
  }
}

int ProfileState::GetAndClearComparisonCount() {
  int result = num_comparisons_;
  num_comparisons_ = 0;
  return result;
}

}  // namespace

/////////////////////////////////
// MediaFileSystemRegistryTest //
/////////////////////////////////

MediaFileSystemRegistryTest::MediaFileSystemRegistryTest()
    : ui_thread_(content::BrowserThread::UI, MessageLoop::current()),
      file_thread_(content::BrowserThread::FILE, MessageLoop::current()) {
}

void MediaFileSystemRegistryTest::CreateProfileState(size_t profile_count) {
  for (size_t i = 0; i < profile_count; ++i) {
    ProfileState* state = new ProfileState(&rph_factory_);
    profile_states_.push_back(state);
  }
}

ProfileState* MediaFileSystemRegistryTest::GetProfileState(size_t i) {
  return profile_states_[i];
}

std::string MediaFileSystemRegistryTest::AddUserGallery(
    MediaStorageUtil::Type type,
    const std::string& unique_id,
    const base::FilePath& path) {
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
  string16 name = path.LossyDisplayName();
  DCHECK(!MediaStorageUtil::IsMediaDevice(device_id));

  for (size_t i = 0; i < profile_states_.size(); ++i) {
    profile_states_[i]->GetMediaGalleriesPrefs()->AddGalleryWithName(
        device_id, name, base::FilePath(), true /*user_added*/);
  }
  return device_id;
}

std::string MediaFileSystemRegistryTest::AttachDevice(
    MediaStorageUtil::Type type,
    const std::string& unique_id,
    const base::FilePath& location) {
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
  DCHECK(MediaStorageUtil::IsRemovableDevice(device_id));
  string16 name = location.LossyDisplayName();
  ProcessAttach(device_id, name, location.value());
  bool user_added = (type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM);
  for (size_t i = 0; i < profile_states_.size(); ++i) {
    profile_states_[i]->GetMediaGalleriesPrefs()->AddGalleryWithName(
        device_id, name, base::FilePath(), user_added);
  }
  MessageLoop::current()->RunUntilIdle();
  return device_id;
}

void MediaFileSystemRegistryTest::DetachDevice(const std::string& device_id) {
  DCHECK(MediaStorageUtil::IsRemovableDevice(device_id));
  ProcessDetach(device_id);
  MessageLoop::current()->RunUntilIdle();
}

void MediaFileSystemRegistryTest::SetGalleryPermission(
    ProfileState* profile_state, extensions::Extension* extension,
    const std::string& device_id, bool has_access) {
  MediaGalleriesPreferences* preferences =
      profile_state->GetMediaGalleriesPrefs();
  MediaGalleryPrefIdSet pref_id =
      preferences->LookUpGalleriesByDeviceId(device_id);
  ASSERT_EQ(1U, pref_id.size());
  preferences->SetGalleryPermissionForExtension(*extension, *pref_id.begin(),
                                                has_access);
}

void MediaFileSystemRegistryTest::AssertAllAutoAddedGalleries() {
  for (size_t i = 0; i < profile_states_.size(); ++i) {
    MediaGalleriesPreferences* prefs =
        profile_states_[0]->GetMediaGalleriesPrefs();

    // Make sure that we have at least one gallery and that they are all
    // auto added galleries.
    const MediaGalleriesPrefInfoMap& galleries = prefs->known_galleries();
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    ASSERT_GT(galleries.size(), 0U);
#endif
    for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
         it != galleries.end();
         ++it) {
      ASSERT_EQ(MediaGalleryPrefInfo::kAutoDetected, it->second.type);
    }
  }
}

void MediaFileSystemRegistryTest::InitForGalleriesInfoTest(
    FSInfoMap* galleries_info) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  // Get all existing gallery names.
  ProfileState* profile_state = GetProfileState(0U);
  *galleries_info = profile_state->GetGalleriesInfo(
      profile_state->all_permission_extension());
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  ASSERT_EQ(3U, galleries_info->size());
#else
  ASSERT_EQ(0U, galleries_info->size());
#endif
}

void MediaFileSystemRegistryTest::CheckNewGalleryInfo(
    ProfileState* profile_state,
    const FSInfoMap& galleries_info,
    const base::FilePath& location,
    bool removable,
    bool media_device) {
  // Get new galleries.
  FSInfoMap new_galleries_info = profile_state->GetGalleriesInfo(
      profile_state->all_permission_extension());
  ASSERT_EQ(galleries_info.size() + 1U, new_galleries_info.size());

  bool found_new = false;
  for (FSInfoMap::const_iterator it = new_galleries_info.begin();
       it != new_galleries_info.end();
       ++it) {
    if (ContainsKey(galleries_info, it->first))
      continue;

    ASSERT_FALSE(found_new);
    CheckGalleryJSONName(it->second.name, removable);
    CheckGalleryInfo(it->second, test_file_system_context_, location, removable,
                     media_device);
    found_new = true;
  }
  ASSERT_TRUE(found_new);
}

std::vector<MediaFileSystemInfo>
MediaFileSystemRegistryTest::GetAutoAddedGalleries(
    ProfileState* profile_state) {
  const MediaGalleriesPrefInfoMap& galleries =
      profile_state->GetMediaGalleriesPrefs()->known_galleries();
  std::vector<MediaFileSystemInfo> result;
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end();
       ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      base::FilePath path = it->second.AbsolutePath();
      MediaFileSystemInfo info(path.AsUTF8Unsafe(), path, std::string(),
                               0, std::string(), false, false);
      result.push_back(info);
    }
  }
  std::sort(result.begin(), result.end(), MediaFileSystemInfoComparator);
  return result;
}

void MediaFileSystemRegistryTest::SetUp() {
#if defined(OS_WIN)
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableMediaTransferProtocolDeviceOperations);
  test::TestPortableDeviceWatcherWin* portable_device_watcher =
      new test::TestPortableDeviceWatcherWin;
  portable_device_watcher->set_use_dummy_mtp_storage_info(true);
  window_.reset(new test::TestRemovableDeviceNotificationsWindowWin(
      new test::TestVolumeMountWatcherWin, portable_device_watcher));
  window_->Init();
#endif

  ChromeRenderViewHostTestHarness::SetUp();
  DeleteContents();
  SetRenderProcessHostFactory(&rph_factory_);

  test_file_system_context_ = new TestMediaFileSystemContext(
      g_browser_process->media_file_system_registry());
  (new extensions::BackgroundManifestHandler)->Register();

  ASSERT_TRUE(galleries_dir_.CreateUniqueTempDir());
  empty_dir_ = galleries_dir_.path().AppendASCII("empty");
  ASSERT_TRUE(file_util::CreateDirectory(empty_dir_));
  dcim_dir_ = galleries_dir_.path().AppendASCII("with_dcim");
  ASSERT_TRUE(file_util::CreateDirectory(dcim_dir_));
  ASSERT_TRUE(file_util::CreateDirectory(dcim_dir_.Append(kDCIMDirectoryName)));
}

void MediaFileSystemRegistryTest::TearDown() {
  profile_states_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  EXPECT_EQ(0U, registry->GetExtensionGalleriesHostCountForTests());
  BrowserThread::GetBlockingPool()->FlushForTesting();
  MessageLoop::current()->RunUntilIdle();
  extensions::ManifestHandler::ClearRegistryForTesting();
}

///////////
// Tests //
///////////

TEST_F(MediaFileSystemRegistryTest, Basic) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);
  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  std::vector<MediaFileSystemInfo> empty_expectation;
  profile_state->CheckGalleries("basic", empty_expectation, auto_galleries);
}

TEST_F(MediaFileSystemRegistryTest, UserAddedGallery) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();
  ProfileState* profile_state = GetProfileState(0);
  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  std::vector<MediaFileSystemInfo> added_galleries;
  profile_state->CheckGalleries("user added init", added_galleries,
                                auto_galleries);

  // Add a user gallery to the regular permission extension.
  std::string device_id = AddUserGallery(MediaStorageUtil::FIXED_MASS_STORAGE,
                                         empty_dir().AsUTF8Unsafe(),
                                         empty_dir());
  SetGalleryPermission(profile_state,
                       profile_state->regular_permission_extension(),
                       device_id,
                       true /*has access*/);
  MediaFileSystemInfo added_info(empty_dir().AsUTF8Unsafe(), empty_dir(),
                                 std::string(), 0, std::string(), false, false);
  added_galleries.push_back(added_info);
  profile_state->CheckGalleries("user added regular", added_galleries,
                                auto_galleries);

  // Add it to the all galleries extension.
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       device_id,
                       true /*has access*/);
  auto_galleries.push_back(added_info);
  profile_state->CheckGalleries("user added all", added_galleries,
                                auto_galleries);
}

// Regression test to make sure erasing galleries does not result a crash.
TEST_F(MediaFileSystemRegistryTest, EraseGalleries) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);
  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  std::vector<MediaFileSystemInfo> empty_expectation;
  profile_state->CheckGalleries("erase", empty_expectation, auto_galleries);

  MediaGalleriesPreferences* prefs = profile_state->GetMediaGalleriesPrefs();
  MediaGalleriesPrefInfoMap galleries = prefs->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end(); ++it) {
    prefs->ForgetGalleryById(it->first);
  }
}

// Regression test to make sure calling GetPreferences() does not re-insert
// galleries on auto-detected removable devices that were blacklisted.
TEST_F(MediaFileSystemRegistryTest,
       GetPreferencesDoesNotReinsertBlacklistedGalleries) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);
  const size_t gallery_count = GetAutoAddedGalleries(profile_state).size();

  // Attach a device.
  const std::string device_id = AttachDevice(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "removable_dcim_fake_id",
      dcim_dir());
  EXPECT_EQ(gallery_count + 1, GetAutoAddedGalleries(profile_state).size());

  // Forget the device.
  bool forget_gallery = false;
  MediaGalleriesPreferences* prefs =
      GetMediaFileSystemRegistry()->GetPreferences(profile_state->profile());
  const MediaGalleriesPrefInfoMap& galleries = prefs->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end(); ++it) {
    if (it->second.device_id == device_id) {
      prefs->ForgetGalleryById(it->first);
      forget_gallery = true;
      break;
    }
  }
  EXPECT_TRUE(forget_gallery);
  EXPECT_EQ(gallery_count, GetAutoAddedGalleries(profile_state).size());

  // Call GetPreferences() and the gallery count should not change.
  GetMediaFileSystemRegistry()->GetPreferences(profile_state->profile());
  EXPECT_EQ(gallery_count, GetAutoAddedGalleries(profile_state).size());
}

TEST_F(MediaFileSystemRegistryTest, GalleryNameDefault) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  for (FSInfoMap::const_iterator it = galleries_info.begin();
       it != galleries_info.end();
       ++it) {
    CheckGalleryJSONName(it->second.name, false /*not removable*/);
  }
}

// TODO(gbillock): Put the platform-specific parts of this test in tests
// for those classes, not here. This test, internally, ends up creating an
// MTP delegate.
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#if !defined(OS_MACOSX)
TEST_F(MediaFileSystemRegistryTest, GalleryNameMTP) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

#if defined(OS_WIN)
  base::FilePath location(
      PortableDeviceWatcherWin::GetStoragePathFromStorageId(
          test::TestPortableDeviceWatcherWin::kStorageUniqueIdA));
#else
  base::FilePath location(FILE_PATH_LITERAL("/mtp_bogus"));
#endif
  AttachDevice(MediaStorageUtil::MTP_OR_PTP, "mtp_fake_id", location);
  CheckNewGalleryInfo(GetProfileState(0U), galleries_info, location,
                      true /*removable*/, true /* media device */);
}
#endif
#endif

TEST_F(MediaFileSystemRegistryTest, GalleryNameDCIM) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  AttachDevice(MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
               "removable_dcim_fake_id",
               dcim_dir());
  CheckNewGalleryInfo(GetProfileState(0U), galleries_info, dcim_dir(),
                      true /*removable*/, true /* media device */);
}

TEST_F(MediaFileSystemRegistryTest, GalleryNameNoDCIM) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  std::string device_id =
      AttachDevice(MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM,
                   empty_dir().AsUTF8Unsafe(),
                   empty_dir());
  std::string device_id2 =
      AddUserGallery(MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM,
                     empty_dir().AsUTF8Unsafe(),
                     empty_dir());
  ASSERT_EQ(device_id, device_id2);
  // Add permission for new non-default gallery.
  ProfileState* profile_state = GetProfileState(0U);
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       device_id,
                       true /*has access*/);
  CheckNewGalleryInfo(profile_state, galleries_info, empty_dir(),
                      true /*removable*/, false /* media device */);
}

TEST_F(MediaFileSystemRegistryTest, GalleryNameUserAddedPath) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  std::string device_id = AddUserGallery(MediaStorageUtil::FIXED_MASS_STORAGE,
                                         empty_dir().AsUTF8Unsafe(),
                                         empty_dir());
  // Add permission for new non-default gallery.
  ProfileState* profile_state = GetProfileState(0U);
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       device_id,
                       true /*has access*/);
  CheckNewGalleryInfo(profile_state, galleries_info, empty_dir(),
                      false /*removable*/, false /* media device */);
}

TEST_F(MediaFileSystemRegistryTest, DetachedDeviceGalleryPath) {
  const std::string device_id = AttachDevice(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "removable_dcim_fake_id",
      dcim_dir());

  MediaGalleryPrefInfo pref_info;
  pref_info.device_id = device_id;
  EXPECT_EQ(dcim_dir().value(), pref_info.AbsolutePath().value());

  MediaGalleryPrefInfo pref_info_with_relpath;
  pref_info_with_relpath.path =
      base::FilePath(FILE_PATH_LITERAL("test_relpath"));
  pref_info_with_relpath.device_id = device_id;
  EXPECT_EQ(dcim_dir().Append(pref_info_with_relpath.path).value(),
            pref_info_with_relpath.AbsolutePath().value());

  DetachDevice(device_id);
  EXPECT_TRUE(pref_info.AbsolutePath().empty());
  EXPECT_TRUE(pref_info_with_relpath.AbsolutePath().empty());
}

}  // namespace chrome
