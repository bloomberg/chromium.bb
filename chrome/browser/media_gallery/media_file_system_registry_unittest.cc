// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry unit tests.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/media_gallery/media_galleries_preferences_factory.h"
#include "chrome/browser/media_gallery/media_galleries_test_util.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "sync/api/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class TestMediaFileSystemContext : public MediaFileSystemContext {
 public:
  struct FSInfo {
    FSInfo() {}
    FSInfo(const std::string& device_id, const FilePath& path,
           const std::string& fsid);

    bool operator<(const FSInfo& other) const;

    std::string device_id;
    FilePath path;
    std::string fsid;
  };

  explicit TestMediaFileSystemContext(MediaFileSystemRegistry* registry);
  virtual ~TestMediaFileSystemContext() {}

  // MediaFileSystemContext implementation.
  virtual std::string RegisterFileSystemForMassStorage(
      const std::string& device_id, const FilePath& path) OVERRIDE;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  virtual std::string RegisterFileSystemForMTPDevice(
      const std::string& device_id, const FilePath& path,
      scoped_refptr<ScopedMTPDeviceMapEntry>* entry) OVERRIDE;
#endif

  virtual void RevokeFileSystem(const std::string& fsid) OVERRIDE;

 private:
  std::string AddFSEntry(const std::string& device_id, const FilePath& path);

  MediaFileSystemRegistry* registry_;

  // A counter used to construct mock FSIDs.
  int fsid_;

  // The currently allocated mock file systems.
  std::map<std::string /*fsid*/, FSInfo> file_systems_by_id_;
};

TestMediaFileSystemContext::FSInfo::FSInfo(const std::string& device_id,
                                           const FilePath& path,
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
    const std::string& device_id, const FilePath& path) {
  CHECK(MediaStorageUtil::IsMassStorageDevice(device_id));
  return AddFSEntry(device_id, path);
}

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
std::string TestMediaFileSystemContext::RegisterFileSystemForMTPDevice(
    const std::string& device_id, const FilePath& path,
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

std::string TestMediaFileSystemContext::AddFSEntry(const std::string& device_id,
                                                   const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(path.IsAbsolute());
  DCHECK(!path.ReferencesParent());

  std::string fsid = base::StringPrintf("FSID:%d", ++fsid_);
  FSInfo info(device_id, path, fsid);
  file_systems_by_id_[fsid] = info;
  return fsid;
}

namespace {

class TestMediaStorageUtil : public MediaStorageUtil {
 public:
  static void SetTestingMode();

  static bool GetDeviceInfoFromPathTestFunction(const FilePath& path,
                                                std::string* device_id,
                                                string16* device_name,
                                                FilePath* relative_path);
};

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

  extensions::Extension* all_permission_extension();
  extensions::Extension* regular_permission_extension();

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

class MediaFileSystemRegistryTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFileSystemRegistryTest();
  virtual ~MediaFileSystemRegistryTest() {}

  void CreateProfileState(int profile_count);

  ProfileState* GetProfileState(int i);

  FilePath empty_dir() {
    return empty_dir_;
  }

  FilePath dcim_dir() {
    return dcim_dir_;
  }

  // Create a user added gallery based on the information passed and add it to
  // |profiles|. Returns the device id.
  std::string AddUserGallery(MediaStorageUtil::Type type,
                             const std::string& unique_id,
                             const FilePath& path);

  void AttachDevice(MediaStorageUtil::Type type, const std::string& unique_id,
                    const FilePath& location);

  void DetachDevice(const std::string& device_id);

  void SetGalleryPermission(size_t profile, extensions::Extension* extension,
                            const std::string& device_id, bool has_access);

  void AssertAllAutoAddedGalleries();

  std::vector<MediaFileSystemInfo> GetAutoAddedGalleries(size_t profile);

 protected:
  void SetUp();
  void TearDown();

 private:
  // This makes sure that at least one default gallery exists on the file
  // system.
  EnsureMediaDirectoriesExists media_directories_;

  // Some test gallery directories.
  ScopedTempDir galleries_dir_;
  // An empty directory in |galleries_dir_|
  FilePath empty_dir_;
  // A directory in |galleries_dir_| with a DCIM directory in it.
  FilePath dcim_dir_;

  // MediaFileSystemRegistry owns this.
  TestMediaFileSystemContext* test_file_system_context_;

  // Needed for extension service & friends to work.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  MockProfileSharedRenderProcessHostFactory rph_factory_;

  ScopedVector<ProfileState> profile_states_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistryTest);
};

bool MediaFileSystemInfoComparator(const MediaFileSystemInfo& a,
                                   const MediaFileSystemInfo& b) {
  if (a.name != b.name)
    return a.name < b.name;
  if (a.path.value() != b.path.value())
    return a.path.value() < b.path.value();
  return a.fsid < b.fsid;
}

//////////////////////////
// TestMediaStorageUtil //
//////////////////////////

// static
void TestMediaStorageUtil::SetTestingMode() {
  SetGetDeviceInfoFromPathFunctionForTesting(
      &GetDeviceInfoFromPathTestFunction);
}

// static
bool TestMediaStorageUtil::GetDeviceInfoFromPathTestFunction(
    const FilePath& path, std::string* device_id, string16* device_name,
    FilePath* relative_path) {
  if (device_id)
    *device_id = MakeDeviceId(FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
  if (device_name)
    *device_name = path.BaseName().LossyDisplayName();
  if (relative_path)
    *relative_path = FilePath();
  return true;
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
      CommandLine::ForCurrentProcess(), FilePath(), false);

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

  // No Media Galleries permissions.
  std::vector<MediaFileSystemInfo> empty_expectation;
  MediaFileSystemRegistry::GetInstance()->GetMediaFileSystemsForExtension(
      rvh, no_permissions_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 StringPrintf("%s (no permission)", test.c_str()),
                 base::ConstRef(empty_expectation)));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());

  // Read permission only.
  MediaFileSystemRegistry::GetInstance()->GetMediaFileSystemsForExtension(
      rvh, regular_permission_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 StringPrintf("%s (regular permission)", test.c_str()),
                 base::ConstRef(regular_extension_galleries)));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());

  // All galleries permission.
  MediaFileSystemRegistry::GetInstance()->GetMediaFileSystemsForExtension(
      rvh, all_permission_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 StringPrintf("%s (all permission)", test.c_str()),
                 base::ConstRef(all_extension_galleries)));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());
}

extensions::Extension* ProfileState::all_permission_extension() {
  return all_permission_extension_.get();
}

extensions::Extension* ProfileState::regular_permission_extension() {
  return regular_permission_extension_.get();
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
  EXPECT_EQ(expected.size(), actual.size()) << test;
  for (size_t i = 0; i < expected.size() && i < actual.size(); i++) {
    EXPECT_EQ(expected[i].path.value(), actual[i].path.value()) << test;
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

/////////////////////////////////
// MediaFileSystemRegistryTest //
/////////////////////////////////

MediaFileSystemRegistryTest::MediaFileSystemRegistryTest()
    : ui_thread_(content::BrowserThread::UI, MessageLoop::current()),
      file_thread_(content::BrowserThread::FILE, MessageLoop::current()) {
}

void MediaFileSystemRegistryTest::CreateProfileState(int profile_count) {
  for (int i = 0; i < profile_count; i++) {
    ProfileState * state = new ProfileState(&rph_factory_);
    profile_states_.push_back(state);
  }
}

ProfileState* MediaFileSystemRegistryTest::GetProfileState(int i) {
  return profile_states_[i];
}

std::string MediaFileSystemRegistryTest::AddUserGallery(
    MediaStorageUtil::Type type,
    const std::string& unique_id,
    const FilePath& path) {
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
  string16 name = path.LossyDisplayName();
  DCHECK(!MediaStorageUtil::IsMediaDevice(device_id));

  for (size_t i = 0; i < profile_states_.size(); i++) {
    profile_states_[i]->GetMediaGalleriesPrefs()->AddGallery(
        device_id, name, FilePath(), true /*user_added*/);
  }
  return device_id;
}

void MediaFileSystemRegistryTest::AttachDevice(MediaStorageUtil::Type type,
                                               const std::string& unique_id,
                                               const FilePath& location) {
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
  DCHECK(MediaStorageUtil::IsRemovableDevice(device_id));
  string16 name = location.LossyDisplayName();
  base::SystemMonitor::Get()->ProcessRemovableStorageAttached(device_id, name,
                                                              location.value());
}

void MediaFileSystemRegistryTest::DetachDevice(const std::string& device_id) {
  DCHECK(MediaStorageUtil::IsRemovableDevice(device_id));
  base::SystemMonitor::Get()->ProcessRemovableStorageDetached(device_id);
}

void MediaFileSystemRegistryTest::SetGalleryPermission(
    size_t profile, extensions::Extension* extension,
    const std::string& device_id, bool has_access) {
  MediaGalleriesPreferences* preferences =
      GetProfileState(profile)->GetMediaGalleriesPrefs();
  MediaGalleryPrefIdSet pref_id =
      preferences->LookUpGalleriesByDeviceId(device_id);
  DCHECK_EQ(1U, pref_id.size());
  preferences->SetGalleryPermissionForExtension(*extension, *pref_id.begin(),
                                                has_access);
}

void MediaFileSystemRegistryTest::AssertAllAutoAddedGalleries() {
  for (size_t i = 0; i < profile_states_.size(); i++) {
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

std::vector<MediaFileSystemInfo>
MediaFileSystemRegistryTest::GetAutoAddedGalleries(size_t profile) {
  DCHECK_LT(profile, profile_states_.size());
  const MediaGalleriesPrefInfoMap& galleries =
      GetProfileState(profile)->GetMediaGalleriesPrefs()->known_galleries();
  std::vector<MediaFileSystemInfo> result;
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end();
       ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      FilePath path = it->second.AbsolutePath();
      MediaFileSystemInfo info(std::string(), path, std::string());
      result.push_back(info);
    }
  }
  std::sort(result.begin(), result.end(), MediaFileSystemInfoComparator);
  return result;
}

void MediaFileSystemRegistryTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  DeleteContents();
  SetRenderProcessHostFactory(&rph_factory_);

  TestMediaStorageUtil::SetTestingMode();
  test_file_system_context_ =
      new TestMediaFileSystemContext(MediaFileSystemRegistry::GetInstance());

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
  MediaFileSystemRegistry* registry = MediaFileSystemRegistry::GetInstance();
  EXPECT_EQ(0U, registry->GetExtensionHostCountForTests());
}

///////////
// Tests //
///////////

TEST_F(MediaFileSystemRegistryTest, Basic) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  std::vector<MediaFileSystemInfo> empty_expectation;
  std::vector<MediaFileSystemInfo> auto_galleries = GetAutoAddedGalleries(0);
  GetProfileState(0)->CheckGalleries("basic", empty_expectation,
                                     auto_galleries);
}

TEST_F(MediaFileSystemRegistryTest, UserAddedGallery) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  std::vector<MediaFileSystemInfo> added_galleries;
  std::vector<MediaFileSystemInfo> auto_galleries = GetAutoAddedGalleries(0);
  GetProfileState(0)->CheckGalleries("user added init", added_galleries,
                                     auto_galleries);

  // Add a user gallery to the regular permission extension.
  std::string device_id = AddUserGallery(MediaStorageUtil::FIXED_MASS_STORAGE,
                                         empty_dir().AsUTF8Unsafe(),
                                         empty_dir());
  SetGalleryPermission(0,
                       GetProfileState(0)->regular_permission_extension(),
                       device_id,
                       true /*has access*/);
  MediaFileSystemInfo added_info(empty_dir().AsUTF8Unsafe(), empty_dir(),
                                 std::string());
  added_galleries.push_back(added_info);
  GetProfileState(0)->CheckGalleries("user added regular", added_galleries,
                                     auto_galleries);

  // Add it to the all galleries extension.
  SetGalleryPermission(0, GetProfileState(0)->all_permission_extension(),
                       device_id, true /*has access*/);
  auto_galleries.push_back(added_info);
  GetProfileState(0)->CheckGalleries("user added all", added_galleries,
                                     auto_galleries);
}

}  // namespace

}  // namespace chrome
