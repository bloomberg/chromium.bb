// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rickcam): Bug 73183: Add unit tests for image loading

#include "chrome/browser/background/background_application_list_model.h"

#include <stddef.h>

#include <cstdlib>
#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

// This value is used to seed the PRNG at the beginning of a sequence of
// operations to produce a repeatable sequence.
#define RANDOM_SEED (0x33F7A7A7)

using extensions::APIPermission;
using extensions::Extension;
using extensions::ExtensionRegistry;

// For ExtensionService interface when it requires a path that is not used.
base::FilePath bogus_file_pathname(const std::string& name) {
  return base::FilePath(FILE_PATH_LITERAL("//foobar_nonexistent"))
      .AppendASCII(name);
}

class BackgroundApplicationListModelTest
    : public extensions::ExtensionServiceTestBase {
 public:
  BackgroundApplicationListModelTest() {}
  ~BackgroundApplicationListModelTest() override {}

 protected:
  void InitializeAndLoadEmptyExtensionService() {
    InitializeEmptyExtensionService();
    service_->Init(); /* Sends EXTENSIONS_READY */
  }

  bool IsBackgroundApp(const Extension& app) {
    return BackgroundApplicationListModel::IsBackgroundApp(app,
                                                           profile_.get());
  }
};

enum PushMessagingOption {
  NO_PUSH_MESSAGING,
  PUSH_MESSAGING_PERMISSION,
  PUSH_MESSAGING_BUT_NOT_BACKGROUND
};

// Returns a barebones test Extension object with the specified |name|.  The
// returned extension will include background permission if
// |background_permission| is true.
static scoped_refptr<Extension> CreateExtension(
    const std::string& name,
    bool background_permission) {
  base::DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, name);
  base::ListValue* permissions = new base::ListValue();
  manifest.Set(extensions::manifest_keys::kPermissions, permissions);
  if (background_permission) {
    permissions->Append(new base::StringValue("background"));
  }

  std::string error;
  scoped_refptr<Extension> extension;

  extension = Extension::Create(
      bogus_file_pathname(name),
      extensions::Manifest::INVALID_LOCATION,
      manifest,
      Extension::NO_FLAGS,
      &error);

  // Cannot ASSERT_* here because that attempts an illegitimate return.
  // Cannot EXPECT_NE here because that assumes non-pointers unlike EXPECT_EQ
  EXPECT_TRUE(extension.get() != NULL) << error;
  return extension;
}

namespace {
std::string GenerateUniqueExtensionName() {
  static int uniqueness = 0;
  std::ostringstream output;
  output << "Unique Named Extension " << uniqueness;
  ++uniqueness;
  return output.str();
}

void AddBackgroundPermission(ExtensionService* service,
                             Extension* extension) {
  if (BackgroundApplicationListModel::IsBackgroundApp(*extension,
                                                      service->profile())) {
    return;
  }

  scoped_refptr<Extension> temporary =
      CreateExtension(GenerateUniqueExtensionName(), true);
  extensions::PermissionsUpdater(service->profile())
      .AddPermissions(extension,
                      temporary->permissions_data()->active_permissions());
}

void RemoveBackgroundPermission(ExtensionService* service,
                                Extension* extension) {
  if (!BackgroundApplicationListModel::IsBackgroundApp(*extension,
                                                       service->profile())) {
    return;
  }
  extensions::PermissionsUpdater(service->profile())
      .RemovePermissionsUnsafe(
          extension, extension->permissions_data()->active_permissions());
}
}  // namespace

// Crashes on Mac tryslaves.
// http://crbug.com/165458
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_ExplicitTest DISABLED_ExplicitTest
#else
#define MAYBE_ExplicitTest ExplicitTest
#endif
// With minimal test logic, verifies behavior over an explicit set of
// extensions, of which some are Background Apps and others are not.
TEST_F(BackgroundApplicationListModelTest, MAYBE_ExplicitTest) {
  InitializeAndLoadEmptyExtensionService();
  ASSERT_TRUE(service()->is_ready());
  ASSERT_TRUE(registry()->enabled_extensions().is_empty());
  std::unique_ptr<BackgroundApplicationListModel> model(
      new BackgroundApplicationListModel(profile_.get()));
  ASSERT_EQ(0U, model->size());

  scoped_refptr<Extension> ext1 = CreateExtension("alpha", false);
  scoped_refptr<Extension> ext2 = CreateExtension("bravo", false);
  scoped_refptr<Extension> ext3 = CreateExtension("charlie", false);
  scoped_refptr<Extension> bgapp1 = CreateExtension("delta", true);
  scoped_refptr<Extension> bgapp2 = CreateExtension("echo", true);
  ASSERT_EQ(0U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());

  // Add alternating Extensions and Background Apps
  ASSERT_FALSE(IsBackgroundApp(*ext1.get()));
  service()->AddExtension(ext1.get());
  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());
  ASSERT_TRUE(IsBackgroundApp(*bgapp1.get()));
  service()->AddExtension(bgapp1.get());
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());
  ASSERT_FALSE(IsBackgroundApp(*ext2.get()));
  service()->AddExtension(ext2.get());
  ASSERT_EQ(3U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());
  ASSERT_TRUE(IsBackgroundApp(*bgapp2.get()));
  service()->AddExtension(bgapp2.get());
  ASSERT_EQ(4U, registry()->enabled_extensions().size());
  ASSERT_EQ(2U, model->size());
  ASSERT_FALSE(IsBackgroundApp(*ext3.get()));
  service()->AddExtension(ext3.get());
  ASSERT_EQ(5U, registry()->enabled_extensions().size());
  ASSERT_EQ(2U, model->size());

  // Remove in FIFO order.
  ASSERT_FALSE(IsBackgroundApp(*ext1.get()));
  service()->UninstallExtension(ext1->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing), NULL);
  ASSERT_EQ(4U, registry()->enabled_extensions().size());
  ASSERT_EQ(2U, model->size());
  ASSERT_TRUE(IsBackgroundApp(*bgapp1.get()));
  service()->UninstallExtension(bgapp1->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing), NULL);
  ASSERT_EQ(3U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());
  ASSERT_FALSE(IsBackgroundApp(*ext2.get()));
  service()->UninstallExtension(ext2->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing), NULL);
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());
  ASSERT_TRUE(IsBackgroundApp(*bgapp2.get()));
  service()->UninstallExtension(bgapp2->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing), NULL);
  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());
  ASSERT_FALSE(IsBackgroundApp(*ext3.get()));
  service()->UninstallExtension(ext3->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing), NULL);
  ASSERT_EQ(0U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());
}

// With minimal test logic, verifies behavior with dynamic permissions.
TEST_F(BackgroundApplicationListModelTest, AddRemovePermissionsTest) {
  InitializeAndLoadEmptyExtensionService();
  ASSERT_TRUE(service()->is_ready());
  ASSERT_TRUE(registry()->enabled_extensions().is_empty());
  std::unique_ptr<BackgroundApplicationListModel> model(
      new BackgroundApplicationListModel(profile_.get()));
  ASSERT_EQ(0U, model->size());

  scoped_refptr<Extension> ext = CreateExtension("extension", false);
  ASSERT_FALSE(
      ext->permissions_data()->HasAPIPermission(APIPermission::kBackground));
  scoped_refptr<Extension> bgapp = CreateExtension("application", true);
  ASSERT_TRUE(
      bgapp->permissions_data()->HasAPIPermission(APIPermission::kBackground));
  ASSERT_EQ(0U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());

  // Add one (non-background) extension and one background application
  ASSERT_FALSE(IsBackgroundApp(*ext.get()));
  service()->AddExtension(ext.get());
  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());
  ASSERT_TRUE(IsBackgroundApp(*bgapp.get()));
  service()->AddExtension(bgapp.get());
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());

  // Change permissions back and forth
  AddBackgroundPermission(service(), ext.get());
  ASSERT_TRUE(
      ext->permissions_data()->HasAPIPermission(APIPermission::kBackground));
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(2U, model->size());
  RemoveBackgroundPermission(service(), bgapp.get());
  ASSERT_FALSE(
      bgapp->permissions_data()->HasAPIPermission(APIPermission::kBackground));
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());
  RemoveBackgroundPermission(service(), ext.get());
  ASSERT_FALSE(
      ext->permissions_data()->HasAPIPermission(APIPermission::kBackground));
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(0U, model->size());
  AddBackgroundPermission(service(), bgapp.get());
  ASSERT_TRUE(
      bgapp->permissions_data()->HasAPIPermission(APIPermission::kBackground));
  ASSERT_EQ(2U, registry()->enabled_extensions().size());
  ASSERT_EQ(1U, model->size());
}

typedef std::set<scoped_refptr<Extension> > ExtensionCollection;

namespace {
void AddExtension(ExtensionService* service,
                  ExtensionCollection* extensions,
                  BackgroundApplicationListModel* model,
                  size_t* expected,
                  size_t* count) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
  bool create_background = false;
  if (rand() % 2) {
    create_background = true;
    ++*expected;
  }
  scoped_refptr<Extension> extension =
      CreateExtension(GenerateUniqueExtensionName(), create_background);
  ASSERT_EQ(BackgroundApplicationListModel::IsBackgroundApp(*extension.get(),
                                                            service->profile()),
            create_background);
  extensions->insert(extension);
  ++*count;
  ASSERT_EQ(*count, extensions->size());
  service->AddExtension(extension.get());
  ASSERT_EQ(*count, registry->enabled_extensions().size());
  ASSERT_EQ(*expected, model->size());
}

void RemoveExtension(ExtensionService* service,
                     ExtensionCollection* extensions,
                     BackgroundApplicationListModel* model,
                     size_t* expected,
                     size_t* count) {  // Maybe remove an extension.
  ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
  ExtensionCollection::iterator cursor = extensions->begin();
  if (cursor == extensions->end()) {
    // Nothing to remove.  Just verify accounting.
    ASSERT_EQ(0U, *count);
    ASSERT_EQ(0U, *expected);
    ASSERT_EQ(0U, registry->enabled_extensions().size());
    ASSERT_EQ(0U, model->size());
  } else {
    // Randomly select which extension to remove
    if (extensions->size() > 1) {
      int offset = rand() % (extensions->size() - 1);
      for (int index = 0; index < offset; ++index)
        ++cursor;
    }
    scoped_refptr<Extension> extension = cursor->get();
    std::string id = extension->id();
    if (BackgroundApplicationListModel::IsBackgroundApp(*extension.get(),
                                                        service->profile())) {
      --*expected;
    }
    extensions->erase(cursor);
    --*count;
    ASSERT_EQ(*count, extensions->size());
    service->UninstallExtension(extension->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing), NULL);
    ASSERT_EQ(*count, registry->enabled_extensions().size());
    ASSERT_EQ(*expected, model->size());
  }
}

void TogglePermission(ExtensionService* service,
                      ExtensionCollection* extensions,
                      BackgroundApplicationListModel* model,
                      size_t* expected,
                      size_t* count) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
  ExtensionCollection::iterator cursor = extensions->begin();
  if (cursor == extensions->end()) {
    // Nothing to toggle.  Just verify accounting.
    ASSERT_EQ(0U, *count);
    ASSERT_EQ(0U, *expected);
    ASSERT_EQ(0U, registry->enabled_extensions().size());
    ASSERT_EQ(0U, model->size());
  } else {
    // Randomly select which extension to toggle.
    if (extensions->size() > 1) {
      int offset = rand() % (extensions->size() - 1);
      for (int index = 0; index < offset; ++index)
        ++cursor;
    }
    scoped_refptr<Extension> extension = cursor->get();
    std::string id = extension->id();
    if (BackgroundApplicationListModel::IsBackgroundApp(*extension.get(),
                                                        service->profile())) {
      --*expected;
      ASSERT_EQ(*count, extensions->size());
      RemoveBackgroundPermission(service, extension.get());
      ASSERT_EQ(*count, registry->enabled_extensions().size());
      ASSERT_EQ(*expected, model->size());
    } else {
      ++*expected;
      ASSERT_EQ(*count, extensions->size());
      AddBackgroundPermission(service, extension.get());
      ASSERT_EQ(*count, registry->enabled_extensions().size());
      ASSERT_EQ(*expected, model->size());
    }
  }
}
}  // namespace

// Verifies behavior with a pseudo-randomly generated set of actions: Adding and
// removing extensions, of which some are Background Apps and others are not.
TEST_F(BackgroundApplicationListModelTest, RandomTest) {
  InitializeAndLoadEmptyExtensionService();
  ASSERT_TRUE(service()->is_ready());
  ASSERT_TRUE(registry()->enabled_extensions().is_empty());
  std::unique_ptr<BackgroundApplicationListModel> model(
      new BackgroundApplicationListModel(profile_.get()));
  ASSERT_EQ(0U, model->size());

  static const int kIterations = 20;
  ExtensionCollection extensions;
  size_t count = 0;
  size_t expected = 0;
  srand(RANDOM_SEED);
  for (int index = 0; index < kIterations; ++index) {
    switch (rand() % 3) {
      case 0:
        AddExtension(service(), &extensions, model.get(), &expected, &count);
        break;
      case 1:
        RemoveExtension(service(), &extensions, model.get(), &expected, &count);
        break;
      case 2:
        TogglePermission(service(), &extensions, model.get(), &expected,
                         &count);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}
