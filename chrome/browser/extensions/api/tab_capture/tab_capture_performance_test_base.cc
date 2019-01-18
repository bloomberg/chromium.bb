// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_performance_test_base.h"

#include <stdint.h>

#include <cmath>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/gl/gl_switches.h"

TabCapturePerformanceTestBase::TabCapturePerformanceTestBase() = default;

TabCapturePerformanceTestBase::~TabCapturePerformanceTestBase() = default;

void TabCapturePerformanceTestBase::SetUp() {
  // Because screen capture is involved, require pixel output.
  EnablePixelOutput();

  InProcessBrowserTest::SetUp();
}

void TabCapturePerformanceTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &TabCapturePerformanceTestBase::HandleRequest, base::Unretained(this)));
  const bool did_start = embedded_test_server()->Start();
  CHECK(did_start);
}

void TabCapturePerformanceTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Note: The naming "kUseGpuInTests" is very misleading. It actually means
  // "don't use a software OpenGL implementation." Subclasses will either call
  // UseSoftwareCompositing() to use Chrome's software compositor, or else they
  // won't (which means use the default hardware-accelerated compositor).
  command_line->AppendSwitch(switches::kUseGpuInTests);

  command_line->AppendSwitchASCII(extensions::switches::kWhitelistedExtensionID,
                                  kExtensionId);

  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void TabCapturePerformanceTestBase::LoadExtension(
    const base::FilePath& unpacked_dir) {
  CHECK(!extension_);

  LOG(INFO) << "Loading extension...";
  auto* const extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::TestExtensionRegistryObserver registry_observer(
      extension_registry);
  auto* const extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  extensions::UnpackedInstaller::Create(extension_service)->Load(unpacked_dir);
  extension_ = registry_observer.WaitForExtensionReady();
  CHECK(extension_);
  CHECK_EQ(kExtensionId, extension_->id());
}

void TabCapturePerformanceTestBase::NavigateToTestPage(
    const std::string& test_page_html_content) {
  LOG(INFO) << "Navigating to test page...";
  test_page_to_serve_ = test_page_html_content;
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(kTestWebPageHostname, kTestWebPagePath));
}

base::Value TabCapturePerformanceTestBase::SendMessageToExtension(
    const std::string& json) {
  CHECK(extension_);

  const std::string javascript = base::StringPrintf(
      "new Promise((resolve, reject) => {\n"
      "  chrome.runtime.sendMessage(\n"
      "      '%s',\n"
      "      %s,\n"
      "      response => {\n"
      "        if (!response) {\n"
      "          reject(chrome.runtime.lastError.message);\n"
      "        } else {\n"
      "          resolve(response);\n"
      "        }\n"
      "      });\n"
      "})",
      extension_->id().c_str(), json.c_str());
  LOG(INFO) << "Sending message to extension: " << json;
  auto* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  for (;;) {
    const auto result = content::EvalJs(web_contents, javascript);
    if (result.error.empty()) {
      return result.value.Clone();
    }
    LOG(INFO) << "Race condition: Waiting for extension to come up, before "
                 "'sendMessage' retry...";
    ContinueBrowserFor(kSendMessageRetryPeriod);
  }
  NOTREACHED();
  return base::Value();
}

std::string TabCapturePerformanceTestBase::TraceAndObserve(
    const std::string& category_patterns) {
  LOG(INFO) << "Starting tracing and running for "
            << kObservationPeriod.InSecondsF() << " sec...";
  std::string json_events;
  bool success = tracing::BeginTracing(category_patterns);
  CHECK(success);
  ContinueBrowserFor(kObservationPeriod);
  success = tracing::EndTracing(&json_events);
  CHECK(success);
  LOG(INFO) << "Observation period has completed. Ending tracing...";
  return json_events;
}

// static
base::FilePath TabCapturePerformanceTestBase::GetApiTestDataDir() {
  base::FilePath dir;
  const bool success = base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  CHECK(success);
  return dir.AppendASCII("extensions").AppendASCII("api_test");
}

// static
std::string TabCapturePerformanceTestBase::MakeBase64EncodedGZippedString(
    const std::string& input) {
  std::string gzipped_input;
  compression::GzipCompress(input, &gzipped_input);
  std::string result;
  base::Base64Encode(gzipped_input, &result);

  // Break up the string with newlines to make it easier to handle in the
  // console logs.
  constexpr size_t kMaxLineWidth = 80;
  std::string formatted_result;
  formatted_result.reserve(result.size() + 1 + (result.size() / kMaxLineWidth));
  for (std::string::size_type src_pos = 0; src_pos < result.size();
       src_pos += kMaxLineWidth) {
    formatted_result.append(result, src_pos, kMaxLineWidth);
    formatted_result.append(1, '\n');
  }
  return formatted_result;
}

// static
void TabCapturePerformanceTestBase::ContinueBrowserFor(
    base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

std::unique_ptr<net::test_server::HttpResponse>
TabCapturePerformanceTestBase::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  const GURL& url = request.GetURL();
  if (url.path() == kTestWebPagePath) {
    response->set_content(test_page_to_serve_);
  } else {
    response->set_code(net::HTTP_NOT_FOUND);
  }
  VLOG(1) << __func__ << ": request url=" << url.spec()
          << ", response=" << response->code();
  return response;
}

// static
constexpr base::TimeDelta TabCapturePerformanceTestBase::kObservationPeriod;

// static
constexpr base::TimeDelta
    TabCapturePerformanceTestBase::kSendMessageRetryPeriod;

// static
const char TabCapturePerformanceTestBase::kTestWebPageHostname[] =
    "in-process-perf-test.chromium.org";

// static
const char TabCapturePerformanceTestBase::kTestWebPagePath[] =
    "/test_page.html";

// static
const char TabCapturePerformanceTestBase::kExtensionId[] =
    "ddchlicdkolnonkihahngkmmmjnjlkkf";
