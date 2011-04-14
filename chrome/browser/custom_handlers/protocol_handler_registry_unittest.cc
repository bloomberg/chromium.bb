// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

class FakeDelegate : public ProtocolHandlerRegistry::Delegate {
 public:
  virtual ~FakeDelegate() { }
  virtual void RegisterExternalHandler(const std::string& protocol) {
    registered_protocols_.insert(protocol);
  }
  virtual void DeregisterExternalHandler(const std::string& protocol) {
    registered_protocols_.erase(protocol);
  }

  bool IsExternalHandlerRegistered(const std::string& protocol) {
    return registered_protocols_.find(protocol) != registered_protocols_.end();
  }

  void Reset() {
    registered_protocols_.clear();
  }

 private:
  std::set<std::string> registered_protocols_;
};


class ProtocolHandlerRegistryTest : public testing::Test {
 public:
  FakeDelegate* delegate() const { return delegate_; }
  TestingProfile* profile() const { return profile_.get(); }
  PrefService* pref_service() const { return profile_->GetPrefs(); }
  ProtocolHandlerRegistry* registry() const { return registry_.get(); }

  ProtocolHandler* CreateProtocolHandler(const std::string& protocol,
                                         const GURL& url,
                                         const std::string& title) {
  return ProtocolHandler::CreateProtocolHandler(protocol, url,
      UTF8ToUTF16(title));
  }

  ProtocolHandler* MakeProtocolHandler(const std::string& protocol) {
    return CreateProtocolHandler(protocol, GURL("http://blah.com/%s"),
        protocol);
  }

  ProtocolHandler* TestProtocolHandler() {
    return CreateProtocolHandler("test", GURL("http://test.com/%s"), "Test");
  }

  void ReloadProtocolHandlerRegistry() {
    delegate_ = new FakeDelegate(*delegate_);  // Copy across the delegate.
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->Load();
  }

 private:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->SetPrefService(new TestingPrefService());
    delegate_ = new FakeDelegate();
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->Load();

    ProtocolHandlerRegistry::RegisterPrefs(pref_service());
  }

  FakeDelegate* delegate_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<ProtocolHandlerRegistry> registry_;
};

TEST_F(ProtocolHandlerRegistryTest, AcceptProtocolHandlerHandlesProtocol) {
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
  registry()->OnAcceptRegisterProtocolHandler(TestProtocolHandler());
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, DisableDeregistersProtocolHandlers) {
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("test"));
  registry()->OnAcceptRegisterProtocolHandler(TestProtocolHandler());
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("test"));

  registry()->Disable();
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("test"));
  registry()->Enable();
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("test"));
}

TEST_F(ProtocolHandlerRegistryTest, IgnoreProtocolHandler) {
  registry()->OnIgnoreRegisterProtocolHandler(TestProtocolHandler());
  ASSERT_TRUE(registry()->IsIgnored(TestProtocolHandler()));
  registry()->RemoveIgnoredHandler(TestProtocolHandler());
  ASSERT_FALSE(registry()->IsIgnored(TestProtocolHandler()));
}

TEST_F(ProtocolHandlerRegistryTest, SaveAndLoad) {
  registry()->OnAcceptRegisterProtocolHandler(TestProtocolHandler());
  registry()->OnIgnoreRegisterProtocolHandler(MakeProtocolHandler("stuff"));

  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
  ASSERT_TRUE(registry()->IsIgnored(MakeProtocolHandler("stuff")));
  delegate()->Reset();
  ReloadProtocolHandlerRegistry();
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
  ASSERT_TRUE(registry()->IsIgnored(MakeProtocolHandler("stuff")));
}

TEST_F(ProtocolHandlerRegistryTest, TestEnabledDisabled) {
  registry()->Disable();
  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_TRUE(registry()->enabled());
}

TEST_F(ProtocolHandlerRegistryTest,
    DisallowRegisteringExternallyHandledProtocols) {
  delegate()->RegisterExternalHandler("test");
  ASSERT_FALSE(registry()->CanSchemeBeOverridden("test"));
}
