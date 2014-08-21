// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

void AssertInterceptedIO(
    const GURL& url,
    net::URLRequestJobFactory* interceptor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContext context;
  scoped_ptr<net::URLRequest> request(context.CreateRequest(
      url, net::DEFAULT_PRIORITY, NULL, NULL));
  scoped_refptr<net::URLRequestJob> job =
      interceptor->MaybeCreateJobWithProtocolHandler(
          url.scheme(), request.get(), context.network_delegate());
  ASSERT_TRUE(job.get() != NULL);
}

void AssertIntercepted(
    const GURL& url,
    net::URLRequestJobFactory* interceptor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(AssertInterceptedIO,
                                     url,
                                     base::Unretained(interceptor)));
  base::MessageLoop::current()->RunUntilIdle();
}

// FakeURLRequestJobFactory returns NULL for all job creation requests and false
// for all IsHandled*() requests. FakeURLRequestJobFactory can be chained to
// ProtocolHandlerRegistry::JobInterceptorFactory so the result of
// MaybeCreateJobWithProtocolHandler() indicates whether the
// ProtocolHandlerRegistry properly handled a job creation request.
class FakeURLRequestJobFactory : public net::URLRequestJobFactory {
  // net::URLRequestJobFactory implementation:
  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }
  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
    return false;
  }
  virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
    return false;
  }
  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE {
    return true;
  }
};

void AssertWillHandleIO(
    const std::string& scheme,
    bool expected,
    ProtocolHandlerRegistry::JobInterceptorFactory* interceptor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  interceptor->Chain(scoped_ptr<net::URLRequestJobFactory>(
      new FakeURLRequestJobFactory()));
  ASSERT_EQ(expected, interceptor->IsHandledProtocol(scheme));
  interceptor->Chain(scoped_ptr<net::URLRequestJobFactory>());
}

void AssertWillHandle(
    const std::string& scheme,
    bool expected,
    ProtocolHandlerRegistry::JobInterceptorFactory* interceptor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(AssertWillHandleIO,
                                     scheme,
                                     expected,
                                     base::Unretained(interceptor)));
  base::MessageLoop::current()->RunUntilIdle();
}

base::DictionaryValue* GetProtocolHandlerValue(std::string protocol,
                                               std::string url) {
  base::DictionaryValue* value = new base::DictionaryValue();
  value->SetString("protocol", protocol);
  value->SetString("url", url);
  return value;
}

base::DictionaryValue* GetProtocolHandlerValueWithDefault(std::string protocol,
                                                          std::string url,
                                                          bool is_default) {
  base::DictionaryValue* value = GetProtocolHandlerValue(protocol, url);
  value->SetBoolean("default", is_default);
  return value;
}

class FakeDelegate : public ProtocolHandlerRegistry::Delegate {
 public:
  FakeDelegate() : force_os_failure_(false) {}
  virtual ~FakeDelegate() { }
  virtual void RegisterExternalHandler(const std::string& protocol) OVERRIDE {
    ASSERT_TRUE(
        registered_protocols_.find(protocol) == registered_protocols_.end());
    registered_protocols_.insert(protocol);
  }

  virtual void DeregisterExternalHandler(const std::string& protocol) OVERRIDE {
    registered_protocols_.erase(protocol);
  }

  virtual ShellIntegration::DefaultProtocolClientWorker* CreateShellWorker(
    ShellIntegration::DefaultWebClientObserver* observer,
    const std::string& protocol) OVERRIDE;

  virtual ProtocolHandlerRegistry::DefaultClientObserver* CreateShellObserver(
      ProtocolHandlerRegistry* registry) OVERRIDE;

  virtual void RegisterWithOSAsDefaultClient(
      const std::string& protocol,
      ProtocolHandlerRegistry* reg) OVERRIDE {
    ProtocolHandlerRegistry::Delegate::RegisterWithOSAsDefaultClient(protocol,
                                                                     reg);
    ASSERT_FALSE(IsFakeRegisteredWithOS(protocol));
  }

  virtual bool IsExternalHandlerRegistered(
      const std::string& protocol) OVERRIDE {
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
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE {
    ProtocolHandlerRegistry::DefaultClientObserver::SetDefaultWebClientUIState(
        state);
    if (state == ShellIntegration::STATE_IS_DEFAULT) {
      delegate_->FakeRegisterWithOS(worker_->protocol());
    }
    if (state != ShellIntegration::STATE_PROCESSING) {
      base::MessageLoop::current()->Quit();
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

  virtual ShellIntegration::DefaultWebClientState CheckIsDefault() OVERRIDE {
    if (force_failure_) {
      return ShellIntegration::NOT_DEFAULT;
    } else {
      return ShellIntegration::IS_DEFAULT;
    }
  }

  virtual bool SetAsDefault(bool interactive_permitted) OVERRIDE {
    return true;
  }

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
  explicit NotificationCounter(content::BrowserContext* context)
      : events_(0),
        notification_registrar_() {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
            content::Source<content::BrowserContext>(context));
  }

  int events() { return events_; }
  bool notified() { return events_ > 0; }
  void Clear() { events_ = 0; }
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    ++events_;
  }

  int events_;
  content::NotificationRegistrar notification_registrar_;
};

class QueryProtocolHandlerOnChange
    : public content::NotificationObserver {
 public:
  QueryProtocolHandlerOnChange(content::BrowserContext* context,
                               ProtocolHandlerRegistry* registry)
    : local_registry_(registry),
      called_(false),
      notification_registrar_() {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
            content::Source<content::BrowserContext>(context));
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    std::vector<std::string> output;
    local_registry_->GetRegisteredProtocols(&output);
    called_ = true;
  }

  ProtocolHandlerRegistry* local_registry_;
  bool called_;
  content::NotificationRegistrar notification_registrar_;
};

// URLRequest DCHECKS that the current MessageLoop is IO. It does this because
// it can't check the thread id (since net can't depend on content.) We want
// to harness our tests so all threads use the same loop allowing us to
// guarantee all messages are processed.) By overriding the IsType method
// we basically ignore the supplied message loop type, and instead infer
// our type based on the current thread. GO DEPENDENCY INJECTION!
class TestMessageLoop : public base::MessageLoop {
 public:
  TestMessageLoop() {}
  virtual ~TestMessageLoop() {}
  virtual bool IsType(base::MessageLoop::Type type) const OVERRIDE {
    switch (type) {
      case base::MessageLoop::TYPE_UI:
        return BrowserThread::CurrentlyOn(BrowserThread::UI);
      case base::MessageLoop::TYPE_IO:
        return BrowserThread::CurrentlyOn(BrowserThread::IO);
#if defined(OS_ANDROID)
      case base::MessageLoop::TYPE_JAVA: // fall-through
#endif // defined(OS_ANDROID)
      case base::MessageLoop::TYPE_CUSTOM:
      case base::MessageLoop::TYPE_DEFAULT:
        return !BrowserThread::CurrentlyOn(BrowserThread::UI) &&
               !BrowserThread::CurrentlyOn(BrowserThread::IO);
    }
    return false;
  }
};

}  // namespace

class ProtocolHandlerRegistryTest : public testing::Test {
 protected:
  ProtocolHandlerRegistryTest()
  : ui_thread_(BrowserThread::UI, &loop_),
    file_thread_(BrowserThread::FILE, &loop_),
    io_thread_(BrowserThread::IO, &loop_),
    test_protocol_handler_(CreateProtocolHandler("test", "test")) {}

  FakeDelegate* delegate() const { return delegate_; }
  ProtocolHandlerRegistry* registry() { return registry_.get(); }
  TestingProfile* profile() const { return profile_.get(); }
  const ProtocolHandler& test_protocol_handler() const {
    return test_protocol_handler_;
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
                                        const GURL& url) {
    return ProtocolHandler::CreateProtocolHandler(protocol, url);
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
                                        const std::string& name) {
    return CreateProtocolHandler(protocol, GURL("http://" + name + "/%s"));
  }

  void RecreateRegistry(bool initialize) {
    TeadDownRegistry();
    SetUpRegistry(initialize);
  }

  int InPrefHandlerCount() {
    const base::ListValue* in_pref_handlers =
        profile()->GetPrefs()->GetList(prefs::kRegisteredProtocolHandlers);
    return static_cast<int>(in_pref_handlers->GetSize());
  }

  int InMemoryHandlerCount() {
    int in_memory_handler_count = 0;
    ProtocolHandlerRegistry::ProtocolHandlerMultiMap::iterator it =
        registry()->protocol_handlers_.begin();
    for (; it != registry()->protocol_handlers_.end(); ++it)
      in_memory_handler_count += it->second.size();
    return in_memory_handler_count;
  }

  int InPrefIgnoredHandlerCount() {
    const base::ListValue* in_pref_ignored_handlers =
        profile()->GetPrefs()->GetList(prefs::kIgnoredProtocolHandlers);
    return static_cast<int>(in_pref_ignored_handlers->GetSize());
  }

  int InMemoryIgnoredHandlerCount() {
    int in_memory_ignored_handler_count = 0;
    ProtocolHandlerRegistry::ProtocolHandlerList::iterator it =
        registry()->ignored_protocol_handlers_.begin();
    for (; it != registry()->ignored_protocol_handlers_.end(); ++it)
      in_memory_ignored_handler_count++;
    return in_memory_ignored_handler_count;
  }

  // Returns a new registry, initializing it if |initialize| is true.
  // Caller assumes ownership for the object
  void SetUpRegistry(bool initialize) {
    delegate_ = new FakeDelegate();
    registry_.reset(new ProtocolHandlerRegistry(profile(), delegate()));
    if (initialize) registry_->InitProtocolSettings();
  }

  void TeadDownRegistry() {
    registry_->Shutdown();
    registry_.reset();
    // Registry owns the delegate_ it handles deletion of that object.
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    CHECK(profile_->GetPrefs());
    SetUpRegistry(true);
    test_protocol_handler_ =
        CreateProtocolHandler("test", GURL("http://test.com/%s"));
  }

  virtual void TearDown() {
    TeadDownRegistry();
  }

  TestMessageLoop loop_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<TestingProfile> profile_;
  FakeDelegate* delegate_;  // Registry assumes ownership of delegate_.
  scoped_ptr<ProtocolHandlerRegistry> registry_;
  ProtocolHandler test_protocol_handler_;
};

// ProtocolHandlerRegistryTest tests are flaky on Linux & ChromeOS.
// http://crbug.com/133023
#if defined(OS_LINUX)
#define MAYBE_AcceptProtocolHandlerHandlesProtocol \
    DISABLED_AcceptProtocolHandlerHandlesProtocol
#define MAYBE_DeniedProtocolIsntHandledUntilAccepted \
    DISABLED_DeniedProtocolIsntHandledUntilAccepted
#define MAYBE_TestStartsAsDefault DISABLED_TestStartsAsDefault
#define MAYBE_TestRemoveHandlerRemovesDefault \
    DISABLED_TestRemoveHandlerRemovesDefault
#define MAYBE_TestClearDefaultGetsPropagatedToIO \
    DISABLED_TestClearDefaultGetsPropagatedToIO
#define MAYBE_TestIsHandledProtocolWorksOnIOThread \
    DISABLED_TestIsHandledProtocolWorksOnIOThread
#define MAYBE_TestInstallDefaultHandler \
    DISABLED_TestInstallDefaultHandler
#else
#define MAYBE_AcceptProtocolHandlerHandlesProtocol \
    AcceptProtocolHandlerHandlesProtocol
#define MAYBE_DeniedProtocolIsntHandledUntilAccepted \
    DeniedProtocolIsntHandledUntilAccepted
#define MAYBE_TestStartsAsDefault TestStartsAsDefault
#define MAYBE_TestRemoveHandlerRemovesDefault TestRemoveHandlerRemovesDefault
#define MAYBE_TestClearDefaultGetsPropagatedToIO \
    TestClearDefaultGetsPropagatedToIO
#define MAYBE_TestIsHandledProtocolWorksOnIOThread \
    TestIsHandledProtocolWorksOnIOThread
#define MAYBE_TestInstallDefaultHandler TestInstallDefaultHandler
#endif  // defined(OS_LINUX)

TEST_F(ProtocolHandlerRegistryTest,
       MAYBE_AcceptProtocolHandlerHandlesProtocol) {
  ASSERT_FALSE(registry()->IsHandledProtocol("test"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("test"));
}

TEST_F(ProtocolHandlerRegistryTest,
       MAYBE_DeniedProtocolIsntHandledUntilAccepted) {
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
  ProtocolHandler ph1 = CreateProtocolHandler("test", GURL("http://test/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("test", GURL("http://test/%s"));

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
  RecreateRegistry(true);
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

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestStartsAsDefault) {
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

  RecreateRegistry(true);

  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));

  RecreateRegistry(true);
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
  ProtocolHandler ph1 = CreateProtocolHandler("test", GURL("http://test/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("test", GURL("http://test/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->HasRegisteredEquivalent(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestSilentlyRegisterHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("test", GURL("http://test/1/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("test", GURL("http://test/2/%s"));
  ProtocolHandler ph3 = CreateProtocolHandler("ignore", GURL("http://test/%s"));
  ProtocolHandler ph4 = CreateProtocolHandler("ignore", GURL("http://test/%s"));

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

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestRemoveHandlerRemovesDefault) {
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

// TODO(smckay): This is much more appropriately an integration
// test. Make that so, then update the
// ShellIntegretion{Delegate,Observer,Worker} test classes we use to fully
// isolate this test from the FILE thread.
TEST_F(ProtocolHandlerRegistryTest, TestOSRegistration) {
  ProtocolHandler ph_do1 = CreateProtocolHandler("do", "test1");
  ProtocolHandler ph_do2 = CreateProtocolHandler("do", "test2");
  ProtocolHandler ph_dont = CreateProtocolHandler("dont", "test");

  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("do"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("dont"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do1);
  registry()->OnDenyRegisterProtocolHandler(ph_dont);
  base::MessageLoop::current()->Run();  // FILE thread needs to run.
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
#define MAYBE_TestOSRegistrationFailure DISABLED_TestOSRegistrationFailure
#else
#define MAYBE_TestOSRegistrationFailure TestOSRegistrationFailure
#endif

// TODO(smckay): This is much more appropriately an integration
// test. Make that so, then update the
// ShellIntegretion{Delegate,Observer,Worker} test classes we use to fully
// isolate this test from the FILE thread.
TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestOSRegistrationFailure) {
  ProtocolHandler ph_do = CreateProtocolHandler("do", "test1");
  ProtocolHandler ph_dont = CreateProtocolHandler("dont", "test");

  ASSERT_FALSE(registry()->IsHandledProtocol("do"));
  ASSERT_FALSE(registry()->IsHandledProtocol("dont"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do);
  base::MessageLoop::current()->Run();  // FILE thread needs to run.
  delegate()->set_force_os_failure(true);
  registry()->OnAcceptRegisterProtocolHandler(ph_dont);
  base::MessageLoop::current()->Run();  // FILE thread needs to run.
  ASSERT_TRUE(registry()->IsHandledProtocol("do"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("do").size());
  ASSERT_FALSE(registry()->IsHandledProtocol("dont"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("dont").size());
}

TEST_F(ProtocolHandlerRegistryTest, TestMaybeCreateTaskWorksFromIOThread) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  GURL url("mailto:someone@something.com");

  scoped_ptr<net::URLRequestJobFactory> interceptor(
      registry()->CreateJobInterceptorFactory());
  AssertIntercepted(url, interceptor.get());
}

TEST_F(ProtocolHandlerRegistryTest,
       MAYBE_TestIsHandledProtocolWorksOnIOThread) {
  std::string scheme("mailto");
  ProtocolHandler ph1 = CreateProtocolHandler(scheme, "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory> interceptor(
      registry()->CreateJobInterceptorFactory());
  AssertWillHandle(scheme, true, interceptor.get());
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

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestClearDefaultGetsPropagatedToIO) {
  std::string scheme("mailto");
  ProtocolHandler ph1 = CreateProtocolHandler(scheme, "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault(scheme);

  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory> interceptor(
      registry()->CreateJobInterceptorFactory());
  AssertWillHandle(scheme, false, interceptor.get());
}

TEST_F(ProtocolHandlerRegistryTest, TestLoadEnabledGetsPropogatedToIO) {
  std::string mailto("mailto");
  ProtocolHandler ph1 = CreateProtocolHandler(mailto, "MailtoHandler");
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory> interceptor(
      registry()->CreateJobInterceptorFactory());
  AssertWillHandle(mailto, true, interceptor.get());
  registry()->Disable();
  AssertWillHandle(mailto, false, interceptor.get());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph2.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceNonDefaultHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("http://else.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceRemovesStaleHandlers) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("http://test.com/third/%s"));
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
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("http://other.com/%s"));
  ASSERT_EQ(ph1.url().GetOrigin() == ph2.url().GetOrigin(),
      ph1.IsSameOrigin(ph2));
  ASSERT_EQ(ph1.url().GetOrigin() == ph2.url().GetOrigin(),
      ph2.IsSameOrigin(ph1));
  ASSERT_EQ(ph2.url().GetOrigin() == ph3.url().GetOrigin(),
      ph2.IsSameOrigin(ph3));
  ASSERT_EQ(ph3.url().GetOrigin() == ph2.url().GetOrigin(),
      ph3.IsSameOrigin(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestInstallDefaultHandler) {
  RecreateRegistry(false);
  registry()->AddPredefinedHandler(
      CreateProtocolHandler("test", GURL("http://test.com/%s")));
  registry()->InitProtocolSettings();
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(1), protocols.size());
}

#define URL_p1u1 "http://p1u1.com/%s"
#define URL_p1u2 "http://p1u2.com/%s"
#define URL_p1u3 "http://p1u3.com/%s"
#define URL_p2u1 "http://p2u1.com/%s"
#define URL_p2u2 "http://p2u2.com/%s"
#define URL_p3u1 "http://p3u1.com/%s"

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapRegister) {
  base::ListValue handlers_registered_by_pref;
  base::ListValue handlers_registered_by_policy;

  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("p1", URL_p1u2, true));
  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("p1", URL_p1u1, true));
  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("p1", URL_p1u2, false));

  handlers_registered_by_policy.Append(
      GetProtocolHandlerValueWithDefault("p1", URL_p1u1, false));
  handlers_registered_by_policy.Append(
      GetProtocolHandlerValueWithDefault("p3", URL_p3u1, true));

  profile()->GetPrefs()->Set(prefs::kRegisteredProtocolHandlers,
                             handlers_registered_by_pref);
  profile()->GetPrefs()->Set(prefs::kPolicyRegisteredProtocolHandlers,
                             handlers_registered_by_policy);
  registry()->InitProtocolSettings();

  // Duplicate p1u2 eliminated in memory but not yet saved in pref
  ProtocolHandler p1u1 = CreateProtocolHandler("p1", GURL(URL_p1u1));
  ProtocolHandler p1u2 = CreateProtocolHandler("p1", GURL(URL_p1u2));
  ASSERT_EQ(InPrefHandlerCount(), 3);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_TRUE(registry()->IsDefault(p1u1));
  ASSERT_FALSE(registry()->IsDefault(p1u2));

  ProtocolHandler p2u1 = CreateProtocolHandler("p2", GURL(URL_p2u1));
  registry()->OnDenyRegisterProtocolHandler(p2u1);

  // Duplicate p1u2 saved in pref and a new handler added to pref and memory
  ASSERT_EQ(InPrefHandlerCount(), 3);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_FALSE(registry()->IsDefault(p2u1));

  registry()->RemoveHandler(p1u1);

  // p1u1 removed from user pref but not from memory due to policy.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_TRUE(registry()->IsDefault(p1u1));

  ProtocolHandler p3u1 = CreateProtocolHandler("p3", GURL(URL_p3u1));
  registry()->RemoveHandler(p3u1);

  // p3u1 not removed from memory due to policy and it was never in pref.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_TRUE(registry()->IsDefault(p3u1));

  registry()->RemoveHandler(p1u2);

  // p1u2 removed from user pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 1);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_TRUE(registry()->IsDefault(p1u1));

  ProtocolHandler p1u3 = CreateProtocolHandler("p1", GURL(URL_p1u3));
  registry()->OnAcceptRegisterProtocolHandler(p1u3);

  // p1u3 added to pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_FALSE(registry()->IsDefault(p1u1));
  ASSERT_TRUE(registry()->IsDefault(p1u3));

  registry()->RemoveHandler(p1u3);

  // p1u3 the default handler for p1 removed from user pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 1);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_FALSE(registry()->IsDefault(p1u3));
  ASSERT_TRUE(registry()->IsDefault(p1u1));
  ASSERT_TRUE(registry()->IsDefault(p3u1));
  ASSERT_FALSE(registry()->IsDefault(p2u1));
}

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapIgnore) {
  base::ListValue handlers_ignored_by_pref;
  base::ListValue handlers_ignored_by_policy;

  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("p1", URL_p1u1));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("p1", URL_p1u2));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("p1", URL_p1u2));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("p3", URL_p3u1));

  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("p1", URL_p1u2));
  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("p1", URL_p1u3));
  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("p2", URL_p2u1));

  profile()->GetPrefs()->Set(prefs::kIgnoredProtocolHandlers,
                             handlers_ignored_by_pref);
  profile()->GetPrefs()->Set(prefs::kPolicyIgnoredProtocolHandlers,
                             handlers_ignored_by_policy);
  registry()->InitProtocolSettings();

  // Duplicate p1u2 eliminated in memory but not yet saved in pref
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 5);

  ProtocolHandler p2u2 = CreateProtocolHandler("p2", GURL(URL_p2u2));
  registry()->OnIgnoreRegisterProtocolHandler(p2u2);

  // Duplicate p1u2 eliminated in pref, p2u2 added to pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p2u1 = CreateProtocolHandler("p2", GURL(URL_p2u1));
  registry()->RemoveIgnoredHandler(p2u1);

  // p2u1 installed by policy so cant be removed.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p1u2 = CreateProtocolHandler("p1", GURL(URL_p1u2));
  registry()->RemoveIgnoredHandler(p1u2);

  // p1u2 installed by policy and pref so it is removed from pref and not from
  // memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 3);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p1u1 = CreateProtocolHandler("p1", GURL(URL_p1u1));
  registry()->RemoveIgnoredHandler(p1u1);

  // p1u1 installed by pref so it is removed from pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 2);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 5);

  registry()->RemoveIgnoredHandler(p2u2);

  // p2u2 installed by user so it is removed from pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 1);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);

  registry()->OnIgnoreRegisterProtocolHandler(p2u1);

  // p2u1 installed by user but it is already installed by policy, so it is
  // added to pref.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 2);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);

  registry()->RemoveIgnoredHandler(p2u1);

  // p2u1 installed by user and policy, so it is removed from pref alone.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 1);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);
}
