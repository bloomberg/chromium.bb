// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

namespace {

using SharedURLLoaderFactoryGetterCallback =
    base::OnceCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

network::mojom::NetworkContextPtr CreateNetworkContext() {
  network::mojom::NetworkContextPtr network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  GetNetworkService()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                            std::move(context_params));
  return network_context;
}

int LoadBasicRequestOnIOThread(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;

  SimpleURLLoaderTestHelper simple_loader_helper;
  // Wait for callback on UI thread to avoid nesting IO message loops.
  simple_loader_helper.SetRunLoopQuitThread(BrowserThread::UI);

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          [](network::SimpleURLLoader* loader,
             network::mojom::URLLoaderFactory* factory,
             network::SimpleURLLoader::BodyAsStringCallback
                 body_as_string_callback) {
            loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
                factory, std::move(body_as_string_callback));
          },
          base::Unretained(simple_loader.get()),
          base::Unretained(url_loader_factory),
          simple_loader_helper.GetCallback()));

  simple_loader_helper.WaitForCallback();
  return simple_loader->NetError();
}

int LoadBasicRequestOnUIThread(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;

  SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  return simple_loader->NetError();
}

scoped_refptr<network::SharedURLLoaderFactory> GetSharedFactoryOnIOThread(
    SharedURLLoaderFactoryGetterCallback shared_url_loader_factory_getter) {
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory;
  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          [](SharedURLLoaderFactoryGetterCallback getter,
             scoped_refptr<network::SharedURLLoaderFactory>*
                 shared_factory_ptr) {
            *shared_factory_ptr = std::move(getter).Run();
          },
          std::move(shared_url_loader_factory_getter),
          base::Unretained(&shared_factory)),
      run_loop.QuitClosure());
  run_loop.Run();
  return shared_factory;
}

scoped_refptr<network::SharedURLLoaderFactory> GetSharedFactoryOnIOThread(
    URLLoaderFactoryGetter* url_loader_factory_getter) {
  return GetSharedFactoryOnIOThread(
      base::BindOnce(&URLLoaderFactoryGetter::GetNetworkFactory,
                     base::Unretained(url_loader_factory_getter)));
}

scoped_refptr<network::SharedURLLoaderFactory> GetSharedFactoryOnIOThread(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> info) {
  return GetSharedFactoryOnIOThread(base::BindOnce(
      &network::SharedURLLoaderFactory::Create, std::move(info)));
}

void ReleaseOnIOThread(
    scoped_refptr<network::SharedURLLoaderFactory> shared_factory) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<network::SharedURLLoaderFactory> factory) {
            factory = nullptr;
          },
          std::move(shared_factory)));
}

}  // namespace

// This test source has been excluded from Android as Android doesn't have
// out-of-process Network Service.
class NetworkServiceRestartBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceRestartBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kNetworkService);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&NetworkServiceRestartBrowserTest::MonitorRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  GURL GetTestURL() const {
    // Use '/echoheader' instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    return embedded_test_server()->GetURL("/echoheader");
  }

  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  RenderFrameHostImpl* main_frame() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
  }

  bool CheckCanLoadHttp(const std::string& relative_url) {
    GURL test_url = embedded_test_server()->GetURL(relative_url);
    std::string script(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '");
    script += test_url.spec() +
              "', true);"
              "xhr.onload = function (e) {"
              "  if (xhr.readyState === 4) {"
              "    window.domAutomationController.send(xhr.status === 200);"
              "  }"
              "};"
              "xhr.onerror = function () {"
              "  window.domAutomationController.send(false);"
              "};"
              "xhr.send(null)";
    bool xhr_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), script, &xhr_result);
    return xhr_result && execute_result;
  }

  // Will reuse the single opened windows through the test case.
  bool CheckCanLoadHttpInWindowOpen(const std::string& relative_url) {
    GURL test_url = embedded_test_server()->GetURL(relative_url);
    std::string inject_script = base::StringPrintf(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '%s', true);"
        "xhr.onload = function (e) {"
        "  if (xhr.readyState === 4) {"
        "    window.opener.domAutomationController.send(xhr.status === 200);"
        "  }"
        "};"
        "xhr.onerror = function () {"
        "  window.opener.domAutomationController.send(false);"
        "};"
        "xhr.send(null)",
        test_url.spec().c_str());
    std::string window_open_script = base::StringPrintf(
        "var new_window = new_window || window.open('');"
        "var inject_script = document.createElement('script');"
        "inject_script.innerHTML = \"%s\";"
        "new_window.document.body.appendChild(inject_script);",
        inject_script.c_str());

    bool xhr_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), window_open_script, &xhr_result);
    return xhr_result && execute_result;
  }

  // Workers will live throughout the test case unless terminated.
  bool CheckCanWorkerFetch(const std::string& worker_name,
                           const std::string& relative_url) {
    GURL worker_url =
        embedded_test_server()->GetURL("/workers/worker_common.js");
    GURL fetch_url = embedded_test_server()->GetURL(relative_url);
    std::string script = base::StringPrintf(
        "var workers = workers || {};"
        "var worker_name = '%s';"
        "workers[worker_name] = workers[worker_name] || new Worker('%s');"
        "workers[worker_name].onmessage = evt => {"
        "  if (evt.data != 'wait')"
        "    window.domAutomationController.send(evt.data === 200);"
        "};"
        "workers[worker_name].postMessage(\"eval "
        "  fetch(new Request('%s'))"
        "    .then(res => postMessage(res.status))"
        "    .catch(error => postMessage(error.toString()));"
        "  'wait'"
        "\");",
        worker_name.c_str(), worker_url.spec().c_str(),
        fetch_url.spec().c_str());
    bool fetch_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), script, &fetch_result);
    return fetch_result && execute_result;
  }

  // Terminate and delete the worker.
  bool TerminateWorker(const std::string& worker_name) {
    std::string script = base::StringPrintf(
        "var workers = workers || {};"
        "var worker_name = '%s';"
        "if (workers[worker_name]) {"
        "  workers[worker_name].terminate();"
        "  delete workers[worker_name];"
        "  window.domAutomationController.send(true);"
        "} else {"
        "  window.domAutomationController.send(false);"
        "}",
        worker_name.c_str());
    bool fetch_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), script, &fetch_result);
    return fetch_result && execute_result;
  }

  // Called by |embedded_test_server()|.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    last_request_relative_url_ = request.relative_url;
  }

  std::string last_request_relative_url() const {
    return last_request_relative_url_;
  }

 private:
  std::string last_request_relative_url_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceRestartBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       NetworkServiceProcessRecovery) {
  network::mojom::NetworkContextPtr network_context = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context.get(), GetTestURL()));
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_FALSE(network_context.encountered_error());

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // |network_context| will receive an error notification, but it's not
  // guaranteed to have arrived at this point. Flush the pointer to make sure
  // the notification has been received.
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_TRUE(network_context.encountered_error());
  // Make sure we could get |net::ERR_FAILED| with an invalid |network_context|.
  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequest(network_context.get(), GetTestURL()));

  // NetworkService should restart automatically and return valid interface.
  network::mojom::NetworkContextPtr network_context2 = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context2.get(), GetTestURL()));
  EXPECT_TRUE(network_context2.is_bound());
  EXPECT_FALSE(network_context2.encountered_error());
}

// Make sure |StoragePartitionImpl::GetNetworkContext()| returns valid interface
// after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       StoragePartitionImplGetNetworkContext) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  network::mojom::NetworkContext* old_network_context =
      partition->GetNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(old_network_context, GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // |partition->GetNetworkContext()| should return a valid new pointer after
  // crash.
  EXPECT_NE(old_network_context, partition->GetNetworkContext());
  EXPECT_EQ(net::OK,
            LoadBasicRequest(partition->GetNetworkContext(), GetTestURL()));
}

// Make sure |URLLoaderFactoryGetter| returns valid interface after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       URLLoaderFactoryGetterGetNetworkFactory) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter =
      partition->url_loader_factory_getter();

  scoped_refptr<network::SharedURLLoaderFactory> shared_factory =
      GetSharedFactoryOnIOThread(url_loader_factory_getter.get());
  EXPECT_EQ(net::OK,
            LoadBasicRequestOnIOThread(shared_factory.get(), GetTestURL()));
  ReleaseOnIOThread(std::move(shared_factory));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  url_loader_factory_getter->FlushNetworkInterfaceOnIOThreadForTesting();

  // |url_loader_factory_getter| should be able to get a valid new pointer after
  // crash.
  shared_factory = GetSharedFactoryOnIOThread(url_loader_factory_getter.get());
  EXPECT_EQ(net::OK,
            LoadBasicRequestOnIOThread(shared_factory.get(), GetTestURL()));
  ReleaseOnIOThread(std::move(shared_factory));
}

// Make sure the factory returned from
// |URLLoaderFactoryGetter::GetNetworkFactory()| continues to work after
// crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       BrowserIOSharedURLLoaderFactory) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  scoped_refptr<network::SharedURLLoaderFactory> shared_factory =
      GetSharedFactoryOnIOThread(partition->url_loader_factory_getter().get());

  EXPECT_EQ(net::OK,
            LoadBasicRequestOnIOThread(shared_factory.get(), GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  partition->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  // |shared_factory| should continue to work.
  EXPECT_EQ(net::OK,
            LoadBasicRequestOnIOThread(shared_factory.get(), GetTestURL()));
  ReleaseOnIOThread(std::move(shared_factory));
}

// Make sure the factory returned from
// |URLLoaderFactoryGetter::GetNetworkFactory()| doesn't crash if
// it's called after the StoragePartition is deleted.
// TODO(crbug.com/822585): Disabled since flaky on at least Linux ASAN and
// Android.
IN_PROC_BROWSER_TEST_F(
    NetworkServiceRestartBrowserTest,
    DISABLED_BrowserIOSharedFactoryAfterStoragePartitionGone) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true, nullptr);
  auto* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context.get()));
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory(
      GetSharedFactoryOnIOThread(partition->url_loader_factory_getter().get()));

  EXPECT_EQ(net::OK,
            LoadBasicRequestOnIOThread(shared_factory.get(), GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequestOnIOThread(shared_factory.get(), GetTestURL()));
  ReleaseOnIOThread(std::move(shared_factory));
}

// Make sure basic navigation works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       NavigationURLLoaderBasic) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  partition->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
}

// Make sure basic XHR works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, BasicXHR) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanLoadHttp("/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanLoadHttp("/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Make sure the factory returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcess()| continues to work
// after crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, BrowserUIFactory) {
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  auto* factory = partition->GetURLLoaderFactoryForBrowserProcess().get();

  EXPECT_EQ(net::OK, LoadBasicRequestOnUIThread(factory, GetTestURL()));

  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  EXPECT_EQ(net::OK, LoadBasicRequestOnUIThread(factory, GetTestURL()));
}

// Make sure the factory returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcess()| doesn't crash if
// it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       BrowserUIFactoryAfterStoragePartitionGone) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true, nullptr);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());
  scoped_refptr<network::SharedURLLoaderFactory> factory(
      partition->GetURLLoaderFactoryForBrowserProcess());

  EXPECT_EQ(net::OK, LoadBasicRequestOnUIThread(factory.get(), GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequestOnUIThread(factory.get(), GetTestURL()));
}

// Make sure the factory returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| continues
// to work after crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, BrowserIOFactory) {
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      GetSharedFactoryOnIOThread(
          partition->GetURLLoaderFactoryForBrowserProcessIOThread());

  EXPECT_EQ(net::OK, LoadBasicRequestOnIOThread(shared_url_loader_factory.get(),
                                                GetTestURL()));

  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  static_cast<StoragePartitionImpl*>(partition)
      ->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  EXPECT_EQ(net::OK, LoadBasicRequestOnIOThread(shared_url_loader_factory.get(),
                                                GetTestURL()));
  ReleaseOnIOThread(std::move(shared_url_loader_factory));
}

// Make sure the factory getter returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| doesn't
// crash if it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       BrowserIOFactoryGetterAfterStoragePartitionGone) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true, nullptr);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());
  auto shared_url_loader_factory_info =
      partition->GetURLLoaderFactoryForBrowserProcessIOThread();

  browser_context.reset();

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      GetSharedFactoryOnIOThread(std::move(shared_url_loader_factory_info));

  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequestOnIOThread(shared_url_loader_factory.get(),
                                       GetTestURL()));
  ReleaseOnIOThread(std::move(shared_url_loader_factory));
}

// Make sure the factory returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| doesn't
// crash if it's called after the StoragePartition is deleted.
// TODO(crbug.com/822891): Disabled since flaky.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       DISABLED_BrowserIOFactoryAfterStoragePartitionGone) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true, nullptr);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      GetSharedFactoryOnIOThread(
          partition->GetURLLoaderFactoryForBrowserProcessIOThread());

  EXPECT_EQ(net::OK, LoadBasicRequestOnIOThread(shared_url_loader_factory.get(),
                                                GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequestOnIOThread(shared_url_loader_factory.get(),
                                       GetTestURL()));
  ReleaseOnIOThread(std::move(shared_url_loader_factory));
}

// Make sure the window from |window.open()| can load XHR after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, WindowOpenXHR) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanLoadHttpInWindowOpen("/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanLoadHttpInWindowOpen("/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Make sure worker fetch works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, WorkerFetch) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Make sure multiple workers are tracked correctly and work after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, MultipleWorkerFetch) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title1.html"));
  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  // Both workers should work after crash.
  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title2.html"));
  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");

  // Terminate "worker1". "worker2" shouldn't be affected.
  EXPECT_TRUE(TerminateWorker("worker1"));
  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process again. "worker2" should still work.
  SimulateNetworkServiceCrash();
  partition->FlushNetworkInterfaceForTesting();
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

}  // namespace content
