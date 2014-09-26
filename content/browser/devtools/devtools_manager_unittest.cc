// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
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

  virtual ~TestDevToolsClientHost() {
    EXPECT_TRUE(closed_);
  }

  void Close() {
    EXPECT_FALSE(closed_);
    close_counter++;
    agent_host_->DetachClient();
    closed_ = true;
  }

  virtual void AgentHostClosed(
      DevToolsAgentHost* agent_host, bool replaced) OVERRIDE {
    FAIL();
  }

  virtual void DispatchProtocolMessage(
      DevToolsAgentHost* agent_host, const std::string& message) OVERRIDE {
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
  virtual void RendererUnresponsive(WebContents* source) OVERRIDE {
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
  virtual ~TestTarget() {}

  virtual std::string GetId() const OVERRIDE { return agent_host_->GetId(); }
  virtual std::string GetParentId() const OVERRIDE { return std::string(); }
  virtual std::string GetType() const OVERRIDE { return std::string(); }
  virtual std::string GetTitle() const OVERRIDE {
    return agent_host_->GetTitle();
  }
  virtual std::string GetDescription() const OVERRIDE { return std::string(); }
  virtual GURL GetURL() const OVERRIDE { return agent_host_->GetURL(); }
  virtual GURL GetFaviconURL() const OVERRIDE { return GURL(); }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return base::TimeTicks();
  }
  virtual bool IsAttached() const OVERRIDE { return agent_host_->IsAttached(); }
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE { return agent_host_->Activate(); }
  virtual bool Close() const OVERRIDE { return agent_host_->Close(); }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

class TestDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  virtual ~TestDevToolsManagerDelegate() {}

  virtual void Inspect(BrowserContext* browser_context,
                       DevToolsAgentHost* agent_host) OVERRIDE {}

  virtual void DevToolsAgentStateChanged(DevToolsAgentHost* agent_host,
                                         bool attached) OVERRIDE {}

  virtual base::DictionaryValue* HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) OVERRIDE { return NULL; }

  virtual scoped_ptr<DevToolsTarget> CreateNewTarget(const GURL& url) OVERRIDE {
    return scoped_ptr<DevToolsTarget>();
  }

  virtual void EnumerateTargets(TargetCallback callback) OVERRIDE {
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

  virtual std::string GetPageThumbnailData(const GURL& url) OVERRIDE {
    return std::string();
  }
};

class ContentBrowserClientWithDevTools : public TestContentBrowserClient {
 public:
  explicit ContentBrowserClientWithDevTools(DevToolsManagerDelegate* delegate)
      : delegate_(delegate) {}
  virtual ~ContentBrowserClientWithDevTools() {}

  virtual content::DevToolsManagerDelegate*
      GetDevToolsManagerDelegate() OVERRIDE {
    return delegate_;
  }
 private:
  DevToolsManagerDelegate* delegate_;
};

class TestDevToolsManagerObserver : public DevToolsManager::Observer {
 public:
  TestDevToolsManagerObserver()
      : updates_count_(0) {}
  virtual ~TestDevToolsManagerObserver() {}

  int updates_count() { return updates_count_; }
  const TargetList& target_list() { return target_list_; }

  virtual void TargetListChanged(const TargetList& targets) OVERRIDE {
    updates_count_++;
    target_list_ = targets;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::MessageLoop::QuitClosure());
  }

 private:
  int updates_count_;
  TargetList target_list_;
};

}  // namespace

class DevToolsManagerTest : public RenderViewHostImplTestHarness {
 public:
  DevToolsManagerTest()
      : delegate_(new TestDevToolsManagerDelegate()) {}

 protected:
  virtual void SetUp() OVERRIDE {
    RenderViewHostImplTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
    SetBrowserClientForTesting(new ContentBrowserClientWithDevTools(delegate_));
  }

  TestDevToolsManagerDelegate* delegate_;
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

  virtual void Attach(DevToolsExternalAgentProxy* proxy) OVERRIDE {
    recordEvent("Attach");
  };

  virtual void Detach() OVERRIDE {
    recordEvent("Detach");
  };

  virtual void SendMessageToBackend(const std::string& message) OVERRIDE {
    recordEvent(std::string("SendMessageToBackend.") + message);
  };

 public :
  virtual ~TestExternalAgentDelegate() {
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

TEST_F(DevToolsManagerTest, TestObserver) {
  GURL url1("data:text/html,<body>Body1</body>");
  GURL url2("data:text/html,<body>Body2</body>");
  GURL url3("data:text/html,<body>Body3</body>");

  DevToolsManager* manager = DevToolsManager::GetInstance();
  DevToolsManager::SetObserverThrottleIntervalForTest(
      base::TimeDelta::FromMilliseconds(200));

  contents()->NavigateAndCommit(url1);
  RunAllPendingInMessageLoop();

  TestDevToolsManagerObserver* observer = new TestDevToolsManagerObserver();
  manager->AddObserver(observer);
  RunMessageLoop();
  // Added observer should get an update.
  EXPECT_EQ(1, observer->updates_count());
  EXPECT_EQ(1u, observer->target_list().size());
  EXPECT_EQ(contents(),
            observer->target_list()[0]->GetAgentHost()->GetWebContents());
  EXPECT_EQ(url1.spec(),
            observer->target_list()[0]->GetURL().spec());

  contents()->NavigateAndCommit(url2);
  RunAllPendingInMessageLoop();
  contents()->NavigateAndCommit(url3);
  RunMessageLoop();
  // Updates should be coalesced.
  EXPECT_EQ(2, observer->updates_count());
  EXPECT_EQ(1u, observer->target_list().size());
  EXPECT_EQ(contents(),
            observer->target_list()[0]->GetAgentHost()->GetWebContents());
  EXPECT_EQ(url3.spec(),
            observer->target_list()[0]->GetURL().spec());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(250));
  base::MessageLoop::current()->Run();
  // Check there were no extra updates.
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
  EmbeddedWorkerDevToolsManager::GetInstance()->SharedWorkerCreated(
      1, 1, shared_worker);
  contents()->NavigateAndCommit(url2);

  RunMessageLoop();
  EXPECT_EQ(3, observer->updates_count());
  EXPECT_EQ(2u, observer->target_list().size());
  EXPECT_EQ(contents(),
            observer->target_list()[0]->GetAgentHost()->GetWebContents());
  EXPECT_EQ(url2.spec(),
            observer->target_list()[0]->GetURL().spec());
  EXPECT_EQ(DevToolsAgentHost::TYPE_SHARED_WORKER,
            observer->target_list()[1]->GetAgentHost()->GetType());
  EXPECT_EQ(shared_worker_url.spec(),
            observer->target_list()[1]->GetURL().spec());

  EmbeddedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(1, 1);
  RunMessageLoop();
  EXPECT_EQ(4, observer->updates_count());
  EXPECT_EQ(1u, observer->target_list().size());
  EXPECT_EQ(contents(),
            observer->target_list()[0]->GetAgentHost()->GetWebContents());
  EXPECT_EQ(url2.spec(),
            observer->target_list()[0]->GetURL().spec());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(250));
  base::MessageLoop::current()->Run();
  // Check there were no extra updates.
  EXPECT_EQ(4, observer->updates_count());

  manager->RemoveObserver(observer);
}

}  // namespace content
