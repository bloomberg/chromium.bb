// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rickcam): Bug 73183: Add unit tests for image loading

#include <cstdlib>
#include <set>

#include "chrome/browser/background_application_list_model.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// This value is used to seed the PRNG at the beginning of a sequence of
// operations to produce a repeatable sequence.
#define RANDOM_SEED (0x33F7A7A7)

// For ExtensionService interface when it requires a path that is not used.
FilePath bogus_file_path() {
  return FilePath(FILE_PATH_LITERAL("//foobar_nonexistent"));
}

class BackgroundApplicationListModelTest : public testing::Test {
 public:
  BackgroundApplicationListModelTest();
  ~BackgroundApplicationListModelTest();

  virtual void InitializeEmptyExtensionService();

 protected:
  scoped_ptr<Profile> profile_;
  scoped_refptr<ExtensionService> service_;
  MessageLoop loop_;
  BrowserThread ui_thread_;
};

// The message loop may be used in tests which require it to be an IO loop.
BackgroundApplicationListModelTest::BackgroundApplicationListModelTest()
    : loop_(MessageLoop::TYPE_IO),
      ui_thread_(BrowserThread::UI, &loop_) {
}

BackgroundApplicationListModelTest::~BackgroundApplicationListModelTest() {
  // Drop reference to ExtensionService and TestingProfile, so that they can be
  // destroyed while BrowserThreads and MessageLoop are still around.  They
  // are used in the destruction process.
  service_ = NULL;
  profile_.reset(NULL);
  MessageLoop::current()->RunAllPending();
}

// This is modeled on a similar routine in ExtensionServiceTestBase.
void BackgroundApplicationListModelTest::InitializeEmptyExtensionService() {
  TestingProfile* profile = new TestingProfile();
  profile_.reset(profile);
  service_ = profile->CreateExtensionService(
      CommandLine::ForCurrentProcess(),
      bogus_file_path());
  service_->set_extensions_enabled(true);
  service_->set_show_extensions_prompts(false);
  service_->OnLoadedInstalledExtensions(); /* Sends EXTENSIONS_READY */
}

// Returns a barebones test Extension object with the specified |name|.  The
// returned extension will include background permission iff
// |background_permission| is true.
static scoped_refptr<Extension> CreateExtension(const std::string& name,
                                                bool background_permission) {
  DictionaryValue manifest;
  manifest.SetString(extension_manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extension_manifest_keys::kName, name);
  if (background_permission) {
    ListValue* permissions = new ListValue();
    manifest.Set(extension_manifest_keys::kPermissions, permissions);
    permissions->Append(Value::CreateStringValue("background"));
  }
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      bogus_file_path().AppendASCII(name), Extension::INVALID, manifest, false,
      &error);
  // Cannot ASSERT_* here because that attempts an illegitimate return.
  // Cannot EXPECT_NE here because that assumes non-pointers unlike EXPECT_EQ
  EXPECT_TRUE(extension.get() != NULL) << error;
  return extension;
}

// With minimal test logic, verifies behavior over an explicit set of
// extensions, of which some are Background Apps and others are not.
TEST_F(BackgroundApplicationListModelTest, LoadExplicitExtensions) {
  InitializeEmptyExtensionService();
  ExtensionService* service = profile_->GetExtensionService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(service->is_ready());
  ASSERT_TRUE(service->extensions());
  ASSERT_TRUE(service->extensions()->empty());
  scoped_ptr<BackgroundApplicationListModel> model(
      new BackgroundApplicationListModel(profile_.get()));
  ASSERT_EQ(0U, model->size());

  scoped_refptr<Extension> ext1 = CreateExtension("alpha", false);
  scoped_refptr<Extension> ext2 = CreateExtension("bravo", false);
  scoped_refptr<Extension> ext3 = CreateExtension("charlie", false);
  scoped_refptr<Extension> bgapp1 = CreateExtension("delta", true);
  scoped_refptr<Extension> bgapp2 = CreateExtension("echo", true);
  ASSERT_TRUE(service->extensions() != NULL);
  ASSERT_EQ(0U, service->extensions()->size());
  ASSERT_EQ(0U, model->size());
  // Add alternating Extensions and Background Apps
  ASSERT_FALSE(BackgroundApplicationListModel::IsBackgroundApp(*ext1));
  service->AddExtension(ext1);
  ASSERT_EQ(1U, service->extensions()->size());
  ASSERT_EQ(0U, model->size());
  ASSERT_TRUE(BackgroundApplicationListModel::IsBackgroundApp(*bgapp1));
  service->AddExtension(bgapp1);
  ASSERT_EQ(2U, service->extensions()->size());
  ASSERT_EQ(1U, model->size());
  ASSERT_FALSE(BackgroundApplicationListModel::IsBackgroundApp(*ext2));
  service->AddExtension(ext2);
  ASSERT_EQ(3U, service->extensions()->size());
  ASSERT_EQ(1U, model->size());
  ASSERT_TRUE(BackgroundApplicationListModel::IsBackgroundApp(*bgapp2));
  service->AddExtension(bgapp2);
  ASSERT_EQ(4U, service->extensions()->size());
  ASSERT_EQ(2U, model->size());
  ASSERT_FALSE(BackgroundApplicationListModel::IsBackgroundApp(*ext3));
  service->AddExtension(ext3);
  ASSERT_EQ(5U, service->extensions()->size());
  ASSERT_EQ(2U, model->size());
  // Remove in FIFO order.
  ASSERT_FALSE(BackgroundApplicationListModel::IsBackgroundApp(*ext1));
  service->UninstallExtension(ext1->id(), false);
  ASSERT_EQ(4U, service->extensions()->size());
  ASSERT_EQ(2U, model->size());
  ASSERT_TRUE(BackgroundApplicationListModel::IsBackgroundApp(*bgapp1));
  service->UninstallExtension(bgapp1->id(), false);
  ASSERT_EQ(3U, service->extensions()->size());
  ASSERT_EQ(1U, model->size());
  ASSERT_FALSE(BackgroundApplicationListModel::IsBackgroundApp(*ext2));
  service->UninstallExtension(ext2->id(), false);
  ASSERT_EQ(2U, service->extensions()->size());
  ASSERT_EQ(1U, model->size());
  ASSERT_TRUE(BackgroundApplicationListModel::IsBackgroundApp(*bgapp2));
  service->UninstallExtension(bgapp2->id(), false);
  ASSERT_EQ(1U, service->extensions()->size());
  ASSERT_EQ(0U, model->size());
  ASSERT_FALSE(BackgroundApplicationListModel::IsBackgroundApp(*ext3));
  service->UninstallExtension(ext3->id(), false);
  ASSERT_EQ(0U, service->extensions()->size());
  ASSERT_EQ(0U, model->size());
}

typedef std::set<scoped_refptr<Extension> > ExtensionSet;

namespace {
std::string GenerateUniqueExtensionName() {
  static int uniqueness = 0;
  std::ostringstream output;
  output << "Unique Named Extension " << uniqueness;
  ++uniqueness;
  return output.str();
}
}

// Verifies behavior with a pseudo-randomly generated set of actions: Adding and
// removing extensions, of which some are Background Apps and others are not.
TEST_F(BackgroundApplicationListModelTest, LoadRandomExtension) {
  InitializeEmptyExtensionService();
  ExtensionService* service = profile_->GetExtensionService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(service->is_ready());
  ASSERT_TRUE(service->extensions());
  ASSERT_TRUE(service->extensions()->empty());
  scoped_ptr<BackgroundApplicationListModel> model(
      new BackgroundApplicationListModel(profile_.get()));
  ASSERT_EQ(0U, model->size());

  static const int kIterations = 500;
  ExtensionSet extensions;
  size_t count = 0;
  size_t expected = 0;
  srand(RANDOM_SEED);
  for (int index = 0; index < kIterations; ++index) {
    if (rand() % 2) {  // Add an extension
      std::string name = GenerateUniqueExtensionName();
      bool create_background = false;
      if (rand() % 2) {
        create_background = true;
        ++expected;
      }
      scoped_refptr<Extension> extension =
          CreateExtension(name, create_background);
      ASSERT_EQ(BackgroundApplicationListModel::IsBackgroundApp(*extension),
                create_background);
      extensions.insert(extension);
      ++count;
      ASSERT_EQ(count, extensions.size());
      service->AddExtension(extension);
      ASSERT_EQ(count, service->extensions()->size());
      ASSERT_EQ(expected, model->size());
    } else {  // Maybe remove an extension.
      ExtensionSet::iterator cursor = extensions.begin();
      if (cursor == extensions.end()) {
        // Nothing to remove.  Just verify accounting.
        ASSERT_EQ(0U, count);
        ASSERT_EQ(0U, expected);
        ASSERT_EQ(0U, service->extensions()->size());
        ASSERT_EQ(0U, model->size());
      } else {
        // Randomly select which extension to remove
        if (extensions.size() > 1) {
          int offset = rand() % (extensions.size() - 1);
          for (int index = 0; index < offset; ++index)
            ++cursor;
        }
        scoped_refptr<Extension> extension = cursor->get();
        std::string id = extension->id();
        if (BackgroundApplicationListModel::IsBackgroundApp(*extension))
          --expected;
        extensions.erase(cursor);
        --count;
        ASSERT_EQ(count, extensions.size());
        service->UninstallExtension(extension->id(), false);
        ASSERT_EQ(count, service->extensions()->size());
        ASSERT_EQ(expected, model->size());
      }
    }
  }
}
