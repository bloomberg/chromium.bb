// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_renderer_host.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace {

class FakeDelegate : public ProtocolHandlerRegistry::Delegate {
 public:
  FakeDelegate() : force_os_failure_(false) {}
  virtual ~FakeDelegate() { }
  virtual void RegisterExternalHandler(const std::string& protocol) {
    ASSERT_TRUE(
        registered_protocols_.find(protocol) == registered_protocols_.end());
    registered_protocols_.insert(protocol);
  }

  virtual void DeregisterExternalHandler(const std::string& protocol) {
    registered_protocols_.erase(protocol);
  }

  virtual ShellIntegration::DefaultProtocolClientWorker* CreateShellWorker(
    ShellIntegration::DefaultWebClientObserver* observer,
    const std::string& protocol);

  virtual ProtocolHandlerRegistry::DefaultClientObserver* CreateShellObserver(
      ProtocolHandlerRegistry* registry);

  virtual void RegisterWithOSAsDefaultClient(const std::string& protocol,
                                             ProtocolHandlerRegistry* reg) {
    ProtocolHandlerRegistry::Delegate::RegisterWithOSAsDefaultClient(protocol,
                                                                     reg);
    ASSERT_FALSE(IsFakeRegisteredWithOS(protocol));
  }

  virtual bool IsExternalHandlerRegistered(const std::string& protocol) {
    return registered_protocols_.find(protocol) != registered_protocols_.end();
  }

  bool IsFakeRegisteredWithOS(const std::string& protocol) {
    return os_registered_protocols_.find(protocol) !=
        os_registered_protocols_.end();
  }

  void FakeRegisterWithOS(const std::string& protocol) {
    os_registered_protocols_.insert(protocol);
  }

  void Reset() {
    registered_protocols_.clear();
    os_registered_protocols_.clear();
    force_os_failure_ = false;
  }

  void set_force_os_failure(bool force) { force_os_failure_ = force; }

  bool force_os_failure() { return force_os_failure_; }

 private:
  std::set<std::string> registered_protocols_;
  std::set<std::string> os_registered_protocols_;
  bool force_os_failure_;
};

class FakeClientObserver
    : public ProtocolHandlerRegistry::DefaultClientObserver {
 public:
  FakeClientObserver(ProtocolHandlerRegistry* registry,
                     FakeDelegate* registry_delegate)
      : ProtocolHandlerRegistry::DefaultClientObserver(registry),
        delegate_(registry_delegate) {}

  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) {
    ProtocolHandlerRegistry::DefaultClientObserver::SetDefaultWebClientUIState(
        state);
    if (state == ShellIntegration::STATE_IS_DEFAULT) {
      delegate_->FakeRegisterWithOS(worker_->protocol());
    }
    if (state != ShellIntegration::STATE_PROCESSING) {
      MessageLoop::current()->Quit();
    }
  }

 private:
  FakeDelegate* delegate_;
};

class FakeProtocolClientWorker
    : public ShellIntegration::DefaultProtocolClientWorker {
 public:
  FakeProtocolClientWorker(ShellIntegration::DefaultWebClientObserver* observer,
                           const std::string& protocol,
                           bool force_failure)
      : ShellIntegration::DefaultProtocolClientWorker(observer, protocol),
        force_failure_(force_failure) {}

 private:
  virtual ~FakeProtocolClientWorker() {}

  virtual ShellIntegration::DefaultWebClientState CheckIsDefault() {
    if (force_failure_) {
      return ShellIntegration::NOT_DEFAULT_WEB_CLIENT;
    } else {
      return ShellIntegration::IS_DEFAULT_WEB_CLIENT;
    }
  }

  virtual void SetAsDefault() {}

 private:
  bool force_failure_;
};

ProtocolHandlerRegistry::DefaultClientObserver*
    FakeDelegate::CreateShellObserver(ProtocolHandlerRegistry* registry) {
  return new FakeClientObserver(registry, this);
}

ShellIntegration::DefaultProtocolClientWorker* FakeDelegate::CreateShellWorker(
    ShellIntegration::DefaultWebClientObserver* observer,
    const std::string& protocol) {
  return new FakeProtocolClientWorker(observer, protocol, force_os_failure_);
}

class NotificationCounter : public content::NotificationObserver {
 public:
  explicit NotificationCounter(Profile* profile)
      : events_(0),
        notification_registrar_() {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
        content::Source<Profile>(profile));
  }

  int events() { return events_; }
  bool notified() { return events_ > 0; }
  void Clear() { events_ = 0; }
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    ++events_;
  }

  int events_;
  content::NotificationRegistrar notification_registrar_;
};

class QueryProtocolHandlerOnChange
    : public content::NotificationObserver {
 public:
  QueryProtocolHandlerOnChange(Profile* profile,
                               ProtocolHandlerRegistry* registry)
    : registry_(registry),
      called_(false),
      notification_registrar_() {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
        content::Source<Profile>(profile));
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    std::vector<std::string> output;
    registry_->GetRegisteredProtocols(&output);
    called_ = true;
  }

  ProtocolHandlerRegistry* registry_;
  bool called_;
  content::NotificationRegistrar notification_registrar_;
};

}  // namespace

class ProtocolHandlerRegistryTest : public testing::Test {
 protected:
  ProtocolHandlerRegistryTest()
      : test_protocol_handler_(CreateProtocolHandler("test", "test")) {}

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
    registry_ = NULL;
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->Load();
  }

  void ReloadProtocolHandlerRegistryAndInstallDefaultHandler() {
    delegate_ = new FakeDelegate();
    registry_->Finalize();
    registry_ = NULL;
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->AddPredefinedHandler(CreateProtocolHandler(
        "test", GURL("http://test.com/%s"), "Test"));
    registry_->Load();
  }

  virtual void SetUp() {
    ui_message_loop_.reset(new MessageLoopForUI());
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    MessageLoop::current()));
    io_thread_.reset(new content::TestBrowserThread(BrowserThread::IO));
    io_thread_->StartIOThread();

    file_thread_.reset(new content::TestBrowserThread(BrowserThread::FILE));
    file_thread_->Start();

    profile_.reset(new TestingProfile());
    profile_->SetPrefService(new TestingPrefService());
    delegate_ = new FakeDelegate();
    registry_ = new ProtocolHandlerRegistry(profile(), delegate());
    registry_->Load();
    test_protocol_handler_ =
        CreateProtocolHandler("test", GURL("http://test.com/%s"), "Test");

    ProtocolHandlerRegistry::RegisterPrefs(pref_service());
  }

  virtual void TearDown() {
    registry_->Finalize();
    registry_ = NULL;
    io_thread_->Stop();
    io_thread_.reset(NULL);
    file_thread_->Stop();
    file_thread_.reset(NULL);
    ui_thread_.reset(NULL);
    ui_message_loop_.reset(NULL);
  }

  bool enabled_io() {
    return registry()->enabled_io_;
  }

  scoped_ptr<MessageLoopForUI> ui_message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  scoped_ptr<content::TestBrowserThread> file_thread_;

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

TEST_F(ProtocolHandlerRegistryTest, IgnoreEquivalentProtocolHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", GURL("http://test/%s"),
                                              "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", GURL("http://test/%s"),
                                              "test2");

  registry()->OnIgnoreRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsIgnored(ph1));
  ASSERT_TRUE(registry()->HasIgnoredEquivalent(ph2));

  registry()->RemoveIgnoredHandler(ph1);
  ASSERT_FALSE(registry()->IsIgnored(ph1));
  ASSERT_FALSE(registry()->HasIgnoredEquivalent(ph2));
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

TEST_F(ProtocolHandlerRegistryTest, TestIsEquivalentRegistered) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", GURL("http://test/%s"),
                                              "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", GURL("http://test/%s"),
                                              "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->HasRegisteredEquivalent(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestSilentlyRegisterHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", GURL("http://test/%s"),
                                              "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("test", GURL("http://test/%s"),
                                              "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("ignore", GURL("http://test/%s"),
                                              "ignore1");
  ProtocolHandler ph4 = CreateProtocolHandler("ignore", GURL("http://test/%s"),
                                              "ignore2");

  ASSERT_FALSE(registry()->SilentlyHandleRegisterHandlerRequest(ph1));
  ASSERT_FALSE(registry()->IsRegistered(ph1));

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsRegistered(ph1));

  ASSERT_TRUE(registry()->SilentlyHandleRegisterHandlerRequest(ph2));
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->IsRegistered(ph2));

  ASSERT_FALSE(registry()->SilentlyHandleRegisterHandlerRequest(ph3));
  ASSERT_FALSE(registry()->IsRegistered(ph3));

  registry()->OnIgnoreRegisterProtocolHandler(ph3);
  ASSERT_FALSE(registry()->IsRegistered(ph3));
  ASSERT_TRUE(registry()->IsIgnored(ph3));

  ASSERT_TRUE(registry()->SilentlyHandleRegisterHandlerRequest(ph4));
  ASSERT_FALSE(registry()->IsRegistered(ph4));
  ASSERT_TRUE(registry()->HasIgnoredEquivalent(ph4));
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
  ASSERT_EQ(static_cast<size_t>(3), handlers.size());

  ASSERT_EQ(ph3, handlers[0]);
  ASSERT_EQ(ph2, handlers[1]);
  ASSERT_EQ(ph1, handlers[2]);
}

TEST_F(ProtocolHandlerRegistryTest, TestGetRegisteredProtocols) {
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(0), protocols.size());

  registry()->GetHandlersFor("test");

  protocols.clear();
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(0), protocols.size());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsHandledProtocol) {
  registry()->GetHandlersFor("test");
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest, TestNotifications) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", "test1");
  NotificationCounter counter(profile());

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
  QueryProtocolHandlerOnChange queryer(profile(), registry());
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
  ASSERT_EQ(static_cast<size_t>(1), handled_protocols.size());
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

  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("do"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("dont"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do1);
  registry()->OnDenyRegisterProtocolHandler(ph_dont);
  MessageLoop::current()->Run();
  ASSERT_TRUE(delegate()->IsFakeRegisteredWithOS("do"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("dont"));

  // This should not register with the OS, if it does the delegate
  // will assert for us. We don't need to wait for the message loop
  // as it should not go through to the shell worker.
  registry()->OnAcceptRegisterProtocolHandler(ph_do2);
}

#if defined(OS_LINUX)
// TODO(benwells): When Linux support is more reliable and
// http://crbut.com/88255 is fixed this test will pass.
#define MAYBE_TestOSRegistrationFailure FAILS_TestOSRegistrationFailure
#else
#define MAYBE_TestOSRegistrationFailure TestOSRegistrationFailure
#endif

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestOSRegistrationFailure) {
  ProtocolHandler ph_do = CreateProtocolHandler("do", "test1");
  ProtocolHandler ph_dont = CreateProtocolHandler("dont", "test");

  ASSERT_FALSE(registry()->IsHandledProtocol("do"));
  ASSERT_FALSE(registry()->IsHandledProtocol("dont"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do);
  MessageLoop::current()->Run();
  delegate()->set_force_os_failure(true);
  registry()->OnAcceptRegisterProtocolHandler(ph_dont);
  MessageLoop::current()->Run();
  ASSERT_TRUE(registry()->IsHandledProtocol("do"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("do").size());
  ASSERT_FALSE(registry()->IsHandledProtocol("dont"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("dont").size());
}

static void MakeRequest(const GURL& url, ProtocolHandlerRegistry* registry) {
  net::URLRequest request(url, NULL);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          MessageLoop::QuitClosure());
  scoped_refptr<net::URLRequestJob> job(registry->MaybeCreateJob(&request));
  ASSERT_TRUE(job.get() != NULL);
}

TEST_F(ProtocolHandlerRegistryTest, TestMaybeCreateTaskWorksFromIOThread) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  GURL url("mailto:someone@something.com");
  scoped_refptr<ProtocolHandlerRegistry> r(registry());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(MakeRequest, url, r));
  MessageLoop::current()->Run();
}

static void CheckIsHandled(const std::string& scheme, bool expected,
    ProtocolHandlerRegistry* registry) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          MessageLoop::QuitClosure());
  ASSERT_EQ(expected, registry->IsHandledProtocolIO(scheme));
}

TEST_F(ProtocolHandlerRegistryTest, TestIsHandledProtocolWorksOnIOThread) {
  std::string scheme("mailto");
  ProtocolHandler ph1 = CreateProtocolHandler(scheme, "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  scoped_refptr<ProtocolHandlerRegistry> r(registry());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(CheckIsHandled, scheme, true, r));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemovingDefaultFallsBackToOldDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph2));
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph2);
  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemovingDefaultDoesntChangeHandlers) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  registry()->RemoveHandler(ph3);

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      registry()->GetHandlersFor("mailto");
  ASSERT_EQ(static_cast<size_t>(2), handlers.size());

  ASSERT_EQ(ph2, handlers[0]);
  ASSERT_EQ(ph1, handlers[1]);
}

TEST_F(ProtocolHandlerRegistryTest, TestClearDefaultGetsPropagatedToIO) {
  std::string scheme("mailto");
  ProtocolHandler ph1 = CreateProtocolHandler(scheme, "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault(scheme);
  scoped_refptr<ProtocolHandlerRegistry> r(registry());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(CheckIsHandled, scheme, false, r));
}

static void QuitUILoop() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          MessageLoop::QuitClosure());
}

TEST_F(ProtocolHandlerRegistryTest, TestLoadEnabledGetsPropogatedToIO) {
  registry()->Disable();
  ReloadProtocolHandlerRegistry();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(QuitUILoop));
  MessageLoop::current()->Run();
  ASSERT_FALSE(enabled_io());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto",
      GURL("http://test.com/%s"), "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto",
      GURL("http://test.com/updated-url/%s"), "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph2.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceNonDefaultHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto",
      GURL("http://test.com/%s"), "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto",
      GURL("http://test.com/updated-url/%s"), "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto",
      GURL("http://else.com/%s"), "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceRemovesStaleHandlers) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto",
      GURL("http://test.com/%s"), "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto",
      GURL("http://test.com/updated-url/%s"), "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto",
      GURL("http://test.com/third/%s"), "test");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  // This should replace the previous two handlers.
  ASSERT_TRUE(registry()->AttemptReplace(ph3));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->GetHandlerFor("mailto").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsSameOrigin) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto",
      GURL("http://test.com/%s"), "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto",
      GURL("http://test.com/updated-url/%s"), "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto",
      GURL("http://other.com/%s"), "test");
  ASSERT_EQ(ph1.url().GetOrigin() == ph2.url().GetOrigin(),
      ph1.IsSameOrigin(ph2));
  ASSERT_EQ(ph1.url().GetOrigin() == ph2.url().GetOrigin(),
      ph2.IsSameOrigin(ph1));
  ASSERT_EQ(ph2.url().GetOrigin() == ph3.url().GetOrigin(),
      ph2.IsSameOrigin(ph3));
  ASSERT_EQ(ph3.url().GetOrigin() == ph2.url().GetOrigin(),
      ph3.IsSameOrigin(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestInstallDefaultHandler) {
  ReloadProtocolHandlerRegistryAndInstallDefaultHandler();
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(1), protocols.size());
}
