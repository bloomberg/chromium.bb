// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace content {
namespace {

class TestDevToolsClientHost : public DevToolsAgentHostClient {
 public:
  TestDevToolsClientHost()
      : last_sent_message(NULL),
        closed_(false) {
  }

  ~TestDevToolsClientHost() override { EXPECT_TRUE(closed_); }

  void Close() {
    EXPECT_FALSE(closed_);
    close_counter++;
    agent_host_->DetachClient();
    closed_ = true;
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override {
    FAIL();
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    last_sent_message = &message;
  }

  void InspectAgentHost(DevToolsAgentHost* agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);
  }

  DevToolsAgentHost* agent_host() { return agent_host_.get(); }

  static void ResetCounters() {
    close_counter = 0;
  }

  static int close_counter;

  const std::string* last_sent_message;

 private:
  bool closed_;
  scoped_refptr<DevToolsAgentHost> agent_host_;

  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};

int TestDevToolsClientHost::close_counter = 0;


class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() : renderer_unresponsive_received_(false) {}

  // Notification that the contents is hung.
  void RendererUnresponsive(WebContents* source) override {
    renderer_unresponsive_received_ = true;
  }

  bool renderer_unresponsive_received() const {
    return renderer_unresponsive_received_;
  }

 private:
  bool renderer_unresponsive_received_;
};

class TestTarget : public DevToolsTarget {
 public:
  explicit TestTarget(scoped_refptr<DevToolsAgentHost> agent_host)
      : agent_host_(agent_host) {}
  ~TestTarget() override {}

  std::string GetId() const override { return agent_host_->GetId(); }
  std::string GetParentId() const override { return std::string(); }
  std::string GetType() const override { return std::string(); }
  std::string GetTitle() const override { return agent_host_->GetTitle(); }
  std::string GetDescription() const override { return std::string(); }
  GURL GetURL() const override { return agent_host_->GetURL(); }
  GURL GetFaviconURL() const override { return GURL(); }
  base::TimeTicks GetLastActivityTime() const override {
    return base::TimeTicks();
  }
  bool IsAttached() const override { return agent_host_->IsAttached(); }
  scoped_refptr<DevToolsAgentHost> GetAgentHost() const override {
    return agent_host_;
  }
  bool Activate() const override { return agent_host_->Activate(); }
  bool Close() const override { return agent_host_->Close(); }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

class TestDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  ~TestDevToolsManagerDelegate() override {}

  void Inspect(BrowserContext* browser_context,
               DevToolsAgentHost* agent_host) override {}

  void DevToolsAgentStateChanged(DevToolsAgentHost* agent_host,
                                 bool attached) override {}

  base::DictionaryValue* HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) override {
    return NULL;
  }

  scoped_ptr<DevToolsTarget> CreateNewTarget(const GURL& url) override {
    return scoped_ptr<DevToolsTarget>();
  }

  void EnumerateTargets(TargetCallback callback) override {
    TargetList result;
    DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
    for (DevToolsAgentHost::List::iterator it = agents.begin();
         it != agents.end(); ++it) {
      if ((*it)->GetType() == DevToolsAgentHost::TYPE_WEB_CONTENTS)
        result.insert(result.begin(), new TestTarget(*it));
      else
        result.push_back(new TestTarget(*it));
    }
    callback.Run(result);
  }

  std::string GetPageThumbnailData(const GURL& url) override {
    return std::string();
  }
};

class ContentBrowserClientWithDevTools : public TestContentBrowserClient {
 public:
  ~ContentBrowserClientWithDevTools() override {}
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override {
    return new TestDevToolsManagerDelegate();
  }
};

class TestDevToolsManagerObserver : public DevToolsManager::Observer {
 public:
  TestDevToolsManagerObserver()
      : updates_count_(0) {}
  ~TestDevToolsManagerObserver() override {}

  int updates_count() { return updates_count_; }
  const std::vector<scoped_refptr<DevToolsAgentHost>> hosts() {
    return hosts_;
  }

  void TargetListChanged(const TargetList& targets) override {
    updates_count_++;
    hosts_.clear();
    for (TargetList::const_iterator it = targets.begin();
         it != targets.end(); ++it) {
      hosts_.push_back((*it)->GetAgentHost());
    }
  }

 private:
  int updates_count_;
  std::vector<scoped_refptr<DevToolsAgentHost>> hosts_;
};

}  // namespace

class DevToolsManagerTest : public RenderViewHostImplTestHarness {
 public:
  DevToolsManagerTest()
      : old_browser_client_(NULL) {}

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    DevToolsManager::GetInstance()->SetUpForTest(DevToolsManager::Scheduler());
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    DevToolsManager::GetInstance()->SetUpForTest(DevToolsManager::Scheduler());
    RenderViewHostImplTestHarness::TearDown();
  }

  ContentBrowserClientWithDevTools browser_client_;
  ContentBrowserClient* old_browser_client_;
};

TEST_F(DevToolsManagerTest, OpenAndManuallyCloseDevToolsClientHost) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(web_contents()));
  EXPECT_FALSE(agent->IsAttached());

  TestDevToolsClientHost client_host;
  client_host.InspectAgentHost(agent.get());
  // Test that the connection is established.
  EXPECT_TRUE(agent->IsAttached());
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  client_host.Close();
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
  EXPECT_FALSE(agent->IsAttached());
}

TEST_F(DevToolsManagerTest, NoUnresponsiveDialogInInspectedContents) {
  TestRenderViewHost* inspected_rvh = test_rvh();
  inspected_rvh->set_render_view_created(true);
  EXPECT_FALSE(contents()->GetDelegate());
  TestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  TestDevToolsClientHost client_host;
  scoped_refptr<DevToolsAgentHost> agent_host(DevToolsAgentHost::GetOrCreateFor(
      WebContents::FromRenderViewHost(inspected_rvh)));
  client_host.InspectAgentHost(agent_host.get());

  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(delegate.renderer_unresponsive_received());

  // Now close devtools and check that the notification is delivered.
  client_host.Close();
  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate.renderer_unresponsive_received());

  contents()->SetDelegate(NULL);
}

TEST_F(DevToolsManagerTest, ReattachOnCancelPendingNavigation) {
  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(
      contents()->GetMainFrame(), 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());

  TestDevToolsClientHost client_host;
  client_host.InspectAgentHost(
      DevToolsAgentHost::GetOrCreateFor(web_contents()).get());

  // Navigate to new site which should get a new RenderViewHost.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(client_host.agent_host(),
            DevToolsAgentHost::GetOrCreateFor(web_contents()).get());

  // Interrupt pending navigation and navigate back to the original site.
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(
      contents()->GetMainFrame(), 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(client_host.agent_host(),
            DevToolsAgentHost::GetOrCreateFor(web_contents()).get());
  client_host.Close();
}

class TestExternalAgentDelegate: public DevToolsExternalAgentProxyDelegate {
  std::map<std::string,int> event_counter_;

  void recordEvent(const std::string& name) {
    if (event_counter_.find(name) == event_counter_.end())
      event_counter_[name] = 0;
    event_counter_[name] = event_counter_[name] + 1;
  }

  void expectEvent(int count, const std::string& name) {
    EXPECT_EQ(count, event_counter_[name]);
  }

  void Attach(DevToolsExternalAgentProxy* proxy) override {
    recordEvent("Attach");
  };

  void Detach() override { recordEvent("Detach"); };

  void SendMessageToBackend(const std::string& message) override {
    recordEvent(std::string("SendMessageToBackend.") + message);
  };

 public :
  ~TestExternalAgentDelegate() override {
    expectEvent(1, "Attach");
    expectEvent(1, "Detach");
    expectEvent(0, "SendMessageToBackend.message0");
    expectEvent(1, "SendMessageToBackend.message1");
    expectEvent(2, "SendMessageToBackend.message2");
  }
};

TEST_F(DevToolsManagerTest, TestExternalProxy) {
  TestExternalAgentDelegate* delegate = new TestExternalAgentDelegate();

  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::Create(delegate);
  EXPECT_EQ(agent_host, DevToolsAgentHost::GetForId(agent_host->GetId()));

  TestDevToolsClientHost client_host;
  client_host.InspectAgentHost(agent_host.get());
  agent_host->DispatchProtocolMessage("message1");
  agent_host->DispatchProtocolMessage("message2");
  agent_host->DispatchProtocolMessage("message2");

  client_host.Close();
}

class TestDevToolsManagerScheduler {
 public:
  DevToolsManager::Scheduler callback() {
    return base::Bind(&TestDevToolsManagerScheduler::Schedule,
                      base::Unretained(this));
  }

  void Run() {
    ASSERT_FALSE(closure_.is_null());
    base::Closure copy = closure_;
    closure_.Reset();
    copy.Run();
  }

  bool IsEmpty() {
    return closure_.is_null();
  }

 private:
  void Schedule(base::Closure closure) {
    EXPECT_TRUE(closure_.is_null());
    closure_ = closure;
  }

  base::Closure closure_;
};

TEST_F(DevToolsManagerTest, TestObserver) {
  GURL url1("data:text/html,<body>Body1</body>");
  GURL url2("data:text/html,<body>Body2</body>");
  GURL url3("data:text/html,<body>Body3</body>");

  TestDevToolsManagerScheduler scheduler;
  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->SetUpForTest(scheduler.callback());

  contents()->NavigateAndCommit(url1);
  RunAllPendingInMessageLoop();

  scoped_ptr<TestDevToolsManagerObserver> observer(
      new TestDevToolsManagerObserver());
  manager->AddObserver(observer.get());
  // Added observer should get an update.
  EXPECT_EQ(1, observer->updates_count());
  ASSERT_EQ(1u, observer->hosts().size());
  EXPECT_EQ(contents(), observer->hosts()[0]->GetWebContents());
  EXPECT_EQ(url1.spec(), observer->hosts()[0]->GetURL().spec());

  contents()->NavigateAndCommit(url2);
  RunAllPendingInMessageLoop();
  contents()->NavigateAndCommit(url3);
  scheduler.Run();
  // Updates should be coalesced.
  EXPECT_EQ(2, observer->updates_count());
  ASSERT_EQ(1u, observer->hosts().size());
  EXPECT_EQ(contents(), observer->hosts()[0]->GetWebContents());
  EXPECT_EQ(url3.spec(), observer->hosts()[0]->GetURL().spec());

  // Check there were no extra updates.
  scheduler.Run();
  EXPECT_TRUE(scheduler.IsEmpty());
  EXPECT_EQ(2, observer->updates_count());

  scoped_ptr<WorkerStoragePartition> partition(new WorkerStoragePartition(
      browser_context()->GetRequestContext(),
      NULL, NULL, NULL, NULL, NULL, NULL, NULL));
  WorkerStoragePartitionId partition_id(*partition.get());

  GURL shared_worker_url("http://example.com/shared_worker.js");
  SharedWorkerInstance shared_worker(
      shared_worker_url,
      base::string16(),
      base::string16(),
      blink::WebContentSecurityPolicyTypeReport,
      browser_context()->GetResourceContext(),
      partition_id);
  SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
      1, 1, shared_worker);
  contents()->NavigateAndCommit(url2);

  EXPECT_EQ(3, observer->updates_count());
  ASSERT_EQ(2u, observer->hosts().size());
  EXPECT_EQ(contents(), observer->hosts()[0]->GetWebContents());
  EXPECT_EQ(url2.spec(), observer->hosts()[0]->GetURL().spec());
  EXPECT_EQ(DevToolsAgentHost::TYPE_SHARED_WORKER,
            observer->hosts()[1]->GetType());
  EXPECT_EQ(shared_worker_url.spec(), observer->hosts()[1]->GetURL().spec());

  SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(1, 1);
  scheduler.Run();
  EXPECT_EQ(4, observer->updates_count());
  ASSERT_EQ(1u, observer->hosts().size());
  EXPECT_EQ(contents(), observer->hosts()[0]->GetWebContents());
  EXPECT_EQ(url2.spec(), observer->hosts()[0]->GetURL().spec());

  // Check there were no extra updates.
  scheduler.Run();
  EXPECT_TRUE(scheduler.IsEmpty());
  EXPECT_EQ(4, observer->updates_count());

  manager->RemoveObserver(observer.get());

  EXPECT_TRUE(scheduler.IsEmpty());
}

}  // namespace content
