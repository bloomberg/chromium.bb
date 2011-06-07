// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

class FakeDelegate : public ProtocolHandlerRegistry::Delegate {
 public:
  virtual ~FakeDelegate() { }
  virtual void RegisterExternalHandler(const std::string& protocol) {
    ASSERT_TRUE(
        registered_protocols_.find(protocol) == registered_protocols_.end());
    registered_protocols_.insert(protocol);
  }

  virtual void DeregisterExternalHandler(const std::string& protocol) {
    registered_protocols_.erase(protocol);
  }

  virtual void RegisterWithOSAsDefaultClient(const std::string& protocol) {
    ASSERT_TRUE(os_registered_protocols_.find(protocol) ==
        os_registered_protocols_.end());
    os_registered_protocols_.insert(protocol);
  }

  virtual bool IsExternalHandlerRegistered(const std::string& protocol) {
    return registered_protocols_.find(protocol) != registered_protocols_.end();
  }

  bool IsRegisteredWithOS(const std::string& protocol) {
    return os_registered_protocols_.find(protocol) !=
        os_registered_protocols_.end();
  }

  void Reset() {
    registered_protocols_.clear();
    os_registered_protocols_.clear();
  }

 private:
  std::set<std::string> registered_protocols_;
  std::set<std::string> os_registered_protocols_;
};

class NotificationCounter : public NotificationObserver {
 public:
  NotificationCounter()
      : events_(0),
        notification_registrar_() {
    notification_registrar_.Add(this,
        NotificationType::PROTOCOL_HANDLER_REGISTRY_CHANGED,
        NotificationService::AllSources());
  }

  ~NotificationCounter() {
    notification_registrar_.Remove(this,
        NotificationType::PROTOCOL_HANDLER_REGISTRY_CHANGED,
        NotificationService::AllSources());
  }

  int events() { return events_; }
  bool notified() { return events_ > 0; }
  void Clear() { events_ = 0; }
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ++events_;
  }

  int events_;
  NotificationRegistrar notification_registrar_;
};

class QueryProtocolHandlerOnChange : public NotificationObserver {
 public:
  explicit QueryProtocolHandlerOnChange(ProtocolHandlerRegistry* registry)
    : registry_(registry),
      called_(false),
      notification_registrar_() {
    notification_registrar_.Add(this,
        NotificationType::PROTOCOL_HANDLER_REGISTRY_CHANGED,
        NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    std::vector<std::string> output;
    registry_->GetRegisteredProtocols(&output);
    called_ = true;
  }

  ProtocolHandlerRegistry* registry_;
  bool called_;
  NotificationRegistrar notification_registrar_;
};


class ProtocolHandlerRegistryTest : public RenderViewHostTestHarness {
 protected:
  ProtocolHandlerRegistryTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        test_protocol_handler_(CreateProtocolHandler("test", "test")) {}

  FakeDelegate* delegate() const { return delegate_; }
  TestingProfile* profile() const { return profile_.get(); }
  PrefService* pref_service() const { return profile_->GetPrefs(); }
  ProtocolHandlerRegistry* registry() const { return registry_.get(); }
  const ProtocolHandler& test_protocol_handler() const {
    return test_protocol_handler_;
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
                                        const GURL& url,
                                        const std::string& title) {
  return ProtocolHandler::CreateProtocolHandler(protocol, url,
      UTF8ToUTF16(title));
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
      const std::string& name) {
    return CreateProtocolHandler(protocol, GURL("http://" + name + "/%s"),
        name);
  }

  void ReloadProtocolHandlerRegistry() {
    delegate_ = new FakeDelegate();
    registry_->Finalize();
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->Load();
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->SetPrefService(new TestingPrefService());
    delegate_ = new FakeDelegate();
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->Load();
    test_protocol_handler_ = CreateProtocolHandler("test", GURL("http://test.com/%s"), "Test");

    ProtocolHandlerRegistry::RegisterPrefs(pref_service());
  }

  virtual void TearDown() {
    registry_->Finalize();
  }

  BrowserThread ui_thread_;
  FakeDelegate* delegate_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<ProtocolHandlerRegistry> registry_;
  ProtocolHandler test_protocol_handler_;
};

TEST_F(ProtocolHandlerRegistryTest, AcceptProtocolHandlerHandlesProtocol) {
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, DeniedProtocolIsntHandledUntilAccepted) {
  registry()->OnDenyRegisterProtocolHandler(test_protocol_handler());
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, ClearDefaultMakesProtocolNotHandled) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  registry()->ClearDefault("test");
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
  ASSERT_TRUE(registry()->GetHandlerFor("test").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, DisableDeregistersProtocolHandlers) {
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("test"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("test"));

  registry()->Disable();
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("test"));
  registry()->Enable();
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("test"));
}

TEST_F(ProtocolHandlerRegistryTest, IgnoreProtocolHandler) {
  registry()->OnIgnoreRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsIgnored(test_protocol_handler()));
  registry()->RemoveIgnoredHandler(test_protocol_handler());
  ASSERT_FALSE(registry()->IsIgnored(test_protocol_handler()));
}

TEST_F(ProtocolHandlerRegistryTest, SaveAndLoad) {
  ProtocolHandler stuff_protocol_handler(
      CreateProtocolHandler("stuff", "stuff"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  registry()->OnIgnoreRegisterProtocolHandler(stuff_protocol_handler);

  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
  ASSERT_TRUE(registry()->IsIgnored(stuff_protocol_handler));
  delegate()->Reset();
  ReloadProtocolHandlerRegistry();
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
  ASSERT_TRUE(registry()->IsIgnored(stuff_protocol_handler));
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

TEST_F(ProtocolHandlerRegistryTest, RemovingHandlerMeansItCanBeAddedAgain) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->CanSchemeBeOverridden("test"));
  registry()->RemoveHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->CanSchemeBeOverridden("test"));
}

TEST_F(ProtocolHandlerRegistryTest, TestStartsAsDefault) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsDefault(test_protocol_handler()));
}

TEST_F(ProtocolHandlerRegistryTest, TestClearDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault("test");
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_FALSE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestGetHandlerFor) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_EQ(ph2, registry()->GetHandlerFor("test"));
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, TestMostRecentHandlerIsDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestOnAcceptRegisterProtocolHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsDefault(ph1));
  ASSERT_FALSE(registry()->IsDefault(ph2));

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestDefaultSaveLoad) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  registry()->OnDenyRegisterProtocolHandler(ph1);
  registry()->OnDenyRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->Disable();

  ReloadProtocolHandlerRegistry();
  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));

  ReloadProtocolHandlerRegistry();
  ASSERT_TRUE(registry()->enabled());
}

TEST_F(ProtocolHandlerRegistryTest, TestRemoveHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, TestIsRegistered) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemoveHandlerRemovesDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("test", "test3");

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsDefault(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestGetHandlersFor) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("test", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      registry()->GetHandlersFor("test");
  ASSERT_EQ(ph1, handlers[0]);
  ASSERT_EQ(ph2, handlers[1]);
  ASSERT_EQ(ph3, handlers[2]);
}

TEST_F(ProtocolHandlerRegistryTest, TestGetRegisteredProtocols) {
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ((size_t) 0, protocols.size());

  registry()->GetHandlersFor("test");

  protocols.clear();
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ((size_t) 0, protocols.size());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsHandledProtocol) {
  registry()->GetHandlersFor("test");
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, TestNotifications) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  NotificationCounter counter;

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->Disable();
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->Enable();
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->RemoveHandler(ph1);
  ASSERT_TRUE(counter.notified());
  counter.Clear();
}

TEST_F(ProtocolHandlerRegistryTest, TestReentrantNotifications) {
  QueryProtocolHandlerOnChange queryer(registry());
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(queryer.called_);
}

TEST_F(ProtocolHandlerRegistryTest, TestProtocolsWithNoDefaultAreHandled) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault("test");
  std::vector<std::string> handled_protocols;
  registry()->GetRegisteredProtocols(&handled_protocols);
  ASSERT_EQ((size_t) 1, handled_protocols.size());
  ASSERT_EQ("test", handled_protocols[0]);
}

TEST_F(ProtocolHandlerRegistryTest, TestDisablePreventsHandling) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
  registry()->Disable();
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, TestOSRegistration) {
  ProtocolHandler ph_do1 = CreateProtocolHandler("do", "test1");
  ProtocolHandler ph_do2 = CreateProtocolHandler("do", "test2");
  ProtocolHandler ph_dont = CreateProtocolHandler("dont", "test");

  ASSERT_FALSE(delegate()->IsRegisteredWithOS("do"));
  ASSERT_FALSE(delegate()->IsRegisteredWithOS("dont"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do1);
  ASSERT_TRUE(delegate()->IsRegisteredWithOS("do"));

  // This should not register with the OS, if it does the delegate
  // will assert for us.
  registry()->OnAcceptRegisterProtocolHandler(ph_do2);

  registry()->OnDenyRegisterProtocolHandler(ph_dont);
  ASSERT_FALSE(delegate()->IsRegisteredWithOS("dont"));
}
