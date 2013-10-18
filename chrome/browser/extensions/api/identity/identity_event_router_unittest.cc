// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_event_router.h"

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/permissions/api_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct EventInfo {
  std::string user_id;
  std::string email;
  bool is_signed_in;
};

class FakeEventRouter : public extensions::EventRouter {
 public:
  explicit FakeEventRouter(Profile* profile) : EventRouter(profile, NULL) {}

  virtual void DispatchEventToExtension(
      const std::string& extension_id,
      scoped_ptr<extensions::Event> event) OVERRIDE {
    EventInfo event_info;
    base::DictionaryValue* event_object = NULL;
    EXPECT_TRUE(event->event_args->GetDictionary(0, &event_object));
    EXPECT_TRUE(event_object->GetString("id", &event_info.user_id));
    event_object->GetString("email", &event_info.email);

    EXPECT_TRUE(event->event_args->GetBoolean(1, &event_info.is_signed_in));

    EXPECT_FALSE(ContainsKey(extension_id_to_event_, extension_id));
    extension_id_to_event_[extension_id] = event_info;
  }

  size_t GetEventCount() {
    return extension_id_to_event_.size();
  }

  bool ContainsExtensionId(const std::string extension_id) {
    return ContainsKey(extension_id_to_event_, extension_id);
  }

  const EventInfo& GetEventInfo(const std::string extension_id) {
    return extension_id_to_event_[extension_id];
  }

 private:
  std::map<std::string, EventInfo> extension_id_to_event_;

  DISALLOW_COPY_AND_ASSIGN(FakeEventRouter);
};

class FakeExtensionService : public TestExtensionService {
 public:
  FakeExtensionService() {}
  virtual ~FakeExtensionService() {}

  virtual const ExtensionSet* extensions() const OVERRIDE {
    return &extensions_;
  }

  virtual void AddExtension(const extensions::Extension* extension) OVERRIDE {
    extensions_.Insert(extension);
  }

 private:
  ExtensionSet extensions_;
};

class FakeExtensionSystem : public extensions::TestExtensionSystem {
 public:
  explicit FakeExtensionSystem(Profile* profile)
      : extensions::TestExtensionSystem(profile) {}

  virtual extensions::EventRouter* event_router() OVERRIDE {
    return fake_event_router();
  }

  virtual ExtensionService* extension_service() OVERRIDE {
    ExtensionServiceInterface* as_interface =
        static_cast<ExtensionServiceInterface*>(&fake_extension_service_);
    return static_cast<ExtensionService*>(as_interface);
  }

  FakeEventRouter* fake_event_router() {
    if (!fake_event_router_)
      fake_event_router_.reset(new FakeEventRouter(profile_));
    return fake_event_router_.get();
  }

 private:
  FakeExtensionService fake_extension_service_;
  scoped_ptr<FakeEventRouter> fake_event_router_;

  DISALLOW_COPY_AND_ASSIGN(FakeExtensionSystem);
};

BrowserContextKeyedService* BuildFakeExtensionSystem(
    content::BrowserContext* profile) {
  return new FakeExtensionSystem(static_cast<Profile*>(profile));
}

}  // namespace

namespace extensions {

class IdentityEventRouterTest : public testing::Test {
 public:
  IdentityEventRouterTest()
      : test_profile_(new TestingProfile()),
        identity_event_router_(test_profile_.get()),
        extension_counter_(0) {}

  virtual void SetUp() OVERRIDE {
    fake_extension_system_ = static_cast<FakeExtensionSystem*>(
        ExtensionSystemFactory::GetInstance()->SetTestingFactoryAndUse(
            test_profile_.get(), &BuildFakeExtensionSystem));
  }

  FakeEventRouter* fake_event_router() {
    return fake_extension_system_->fake_event_router();
  }

  Profile* profile() {
    return test_profile_.get();
  }

 protected:
  scoped_refptr<const Extension> CreateExtension(bool has_email_permission) {
    ListBuilder permissions;
    if (has_email_permission)
      permissions.Append("identity.email");

    std::string id = base::StringPrintf("id.%d", extension_counter_++);
    scoped_refptr<const Extension> extension = ExtensionBuilder()
        .SetID(id)
        .SetManifest(DictionaryBuilder()
                     .Set("name", "Extension with ID " + id)
                     .Set("version", "1.0")
                     .Set("manifest_version", 2)
                     .Set("permissions", permissions))
        .Build();
    fake_extension_system_->extension_service()->AddExtension(extension.get());
    fake_event_router()->AddEventListener(
        api::identity::OnSignInChanged::kEventName, NULL, extension->id());
    return extension;
  }

  scoped_ptr<TestingProfile> test_profile_;
  IdentityEventRouter identity_event_router_;
  FakeExtensionSystem* fake_extension_system_;
  content::TestBrowserThreadBundle thread_bundle_;
  int extension_counter_;
};

TEST_F(IdentityEventRouterTest, SignInNoListeners) {
  identity_event_router_.DispatchSignInEvent(
      "test_user_id", "test_email", true);
  EXPECT_EQ(0ul, fake_event_router()->GetEventCount());
}

TEST_F(IdentityEventRouterTest, SignInNoEmailListener) {
  scoped_refptr<const Extension> ext = CreateExtension(false);
  identity_event_router_.DispatchSignInEvent(
      "test_user_id", "test_email", true);
  EXPECT_EQ(1ul, fake_event_router()->GetEventCount());
  EXPECT_TRUE(fake_event_router()->ContainsExtensionId(ext->id()));
  EXPECT_EQ("test_user_id",
            fake_event_router()->GetEventInfo(ext->id()).user_id);
  EXPECT_TRUE(fake_event_router()->GetEventInfo(ext->id()).email.empty());
  EXPECT_TRUE(fake_event_router()->GetEventInfo(ext->id()).is_signed_in);
}

TEST_F(IdentityEventRouterTest, SignInWithEmailListener) {
  scoped_refptr<const Extension> ext = CreateExtension(true);
  identity_event_router_.DispatchSignInEvent(
      "test_user_id", "test_email", true);
  EXPECT_EQ(1ul, fake_event_router()->GetEventCount());
  EXPECT_TRUE(fake_event_router()->ContainsExtensionId(ext->id()));
  EXPECT_EQ("test_user_id",
            fake_event_router()->GetEventInfo(ext->id()).user_id);
  EXPECT_EQ("test_email", fake_event_router()->GetEventInfo(ext->id()).email);
  EXPECT_TRUE(fake_event_router()->GetEventInfo(ext->id()).is_signed_in);
}

TEST_F(IdentityEventRouterTest, SignInMultipleListeners) {
  typedef std::vector<scoped_refptr<const Extension> > ExtensionVector;
  ExtensionVector with_email;
  ExtensionVector no_email;

  for (int i = 0; i < 3; i++)
    with_email.push_back(CreateExtension(true));

  for (int i = 0; i < 2; i++)
    no_email.push_back(CreateExtension(false));

  identity_event_router_.DispatchSignInEvent(
      "test_user_id", "test_email", true);

  EXPECT_EQ(with_email.size() + no_email.size(),
            fake_event_router()->GetEventCount());

  for (ExtensionVector::const_iterator it = with_email.begin();
       it != with_email.end();
       ++it) {
    EXPECT_TRUE(fake_event_router()->ContainsExtensionId((*it)->id()));
    EXPECT_EQ("test_user_id",
              fake_event_router()->GetEventInfo((*it)->id()).user_id);
    EXPECT_EQ("test_email",
              fake_event_router()->GetEventInfo((*it)->id()).email);
    EXPECT_TRUE(fake_event_router()->GetEventInfo((*it)->id()).is_signed_in);
  }

  for (ExtensionVector::const_iterator it = no_email.begin();
       it != no_email.end();
       ++it) {
    EXPECT_TRUE(fake_event_router()->ContainsExtensionId((*it)->id()));
    EXPECT_EQ("test_user_id",
              fake_event_router()->GetEventInfo((*it)->id()).user_id);
    EXPECT_TRUE(fake_event_router()->GetEventInfo((*it)->id()).email.empty());
    EXPECT_TRUE(fake_event_router()->GetEventInfo((*it)->id()).is_signed_in);
  }
}

TEST_F(IdentityEventRouterTest, SignInWithTwoListenersOnOneExtension) {
  scoped_refptr<const Extension> ext = CreateExtension(true);

  scoped_ptr<content::MockRenderProcessHost> fake_render_process(
      new content::MockRenderProcessHost(profile()));
  fake_event_router()->AddEventListener(
      api::identity::OnSignInChanged::kEventName,
      fake_render_process.get(),
      ext->id());

  identity_event_router_.DispatchSignInEvent(
      "test_user_id", "test_email", true);
  EXPECT_EQ(1ul, fake_event_router()->GetEventCount());
  EXPECT_TRUE(fake_event_router()->ContainsExtensionId(ext->id()));
  EXPECT_EQ("test_user_id",
            fake_event_router()->GetEventInfo(ext->id()).user_id);
  EXPECT_EQ("test_email", fake_event_router()->GetEventInfo(ext->id()).email);
  EXPECT_TRUE(fake_event_router()->GetEventInfo(ext->id()).is_signed_in);

  fake_event_router()->RemoveEventListener(
      api::identity::OnSignInChanged::kEventName,
      fake_render_process.get(),
      ext->id());
}

TEST_F(IdentityEventRouterTest, SignOut) {
  scoped_refptr<const Extension> ext = CreateExtension(false);
  identity_event_router_.DispatchSignInEvent(
      "test_user_id", "test_email", false);
  EXPECT_EQ(1ul, fake_event_router()->GetEventCount());
  EXPECT_TRUE(fake_event_router()->ContainsExtensionId(ext->id()));
  EXPECT_EQ("test_user_id",
            fake_event_router()->GetEventInfo(ext->id()).user_id);
  EXPECT_TRUE(fake_event_router()->GetEventInfo(ext->id()).email.empty());
  EXPECT_FALSE(fake_event_router()->GetEventInfo(ext->id()).is_signed_in);
}

}  // namespace extensions
