// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension_id.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!window.scriptExecuted)",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

// Used to monitor requests that reach the RulesetManager.
class URLRequestMonitor : public RulesetManager::TestObserver {
 public:
  explicit URLRequestMonitor(GURL url) : url_(std::move(url)) {}

  // This is called from both the UI and IO thread. Clients should ensure
  // there's no race.
  bool GetAndResetRequestSeen(bool new_val) {
    bool return_val = request_seen_;
    request_seen_ = new_val;
    return return_val;
  }

 private:
  // RulesetManager::TestObserver implementation.
  void OnShouldBlockRequest(const net::URLRequest& request,
                            bool is_incognito_context) override {
    if (request.url() == url_)
      GetAndResetRequestSeen(true);
  }

  GURL url_;
  bool request_seen_ = false;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMonitor);
};

// Helper to set the TestObserver for RulesetManager on the IO thread.
void SetRulesetManagerObserverOnIOThread(RulesetManager::TestObserver* observer,
                                         scoped_refptr<InfoMap> info_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  info_map->GetRulesetManager()->SetObserverForTest(observer);
}

class DeclarativeNetRequestBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<ExtensionLoadType> {
 public:
  DeclarativeNetRequestBrowserTest() {}

  // ExtensionBrowserTest override.
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    base::FilePath test_root_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    test_root_path = test_root_path.AppendASCII("extensions")
                         .AppendASCII("declarative_net_request");
    embedded_test_server()->ServeFilesFromDirectory(test_root_path);

    ASSERT_TRUE(embedded_test_server()->Start());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  content::RenderFrameHost* GetMainFrame(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

  content::RenderFrameHost* GetMainFrame() const {
    return GetMainFrame(browser());
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) const {
    return content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::Bind(&content::FrameMatchesName, name));
  }

  content::PageType GetPageType(Browser* browser) const {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetController()
        .GetLastCommittedEntry()
        ->GetPageType();
  }

  content::PageType GetPageType() const { return GetPageType(browser()); }

  // Loads an extension with the given declarative |rules| in the given
  // |directory|. Generates a fatal failure if the extension failed to load.
  void LoadExtensionWithRules(const std::vector<TestRule>& rules,
                              const std::string& directory) {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    base::HistogramTester tester;

    base::FilePath extension_dir = temp_dir_.GetPath().AppendASCII(directory);
    EXPECT_TRUE(base::CreateDirectory(extension_dir));

    WriteManifestAndRuleset(extension_dir, kJSONRulesetFilepath,
                            kJSONRulesFilename, rules);

    const Extension* extension = nullptr;
    switch (GetParam()) {
      case ExtensionLoadType::PACKED:
        extension = InstallExtension(extension_dir, 1 /* expected_change */);
        break;
      case ExtensionLoadType::UNPACKED:
        extension = LoadExtension(extension_dir);
        break;
    }

    ASSERT_TRUE(extension);
    EXPECT_TRUE(HasValidIndexedRuleset(*extension, profile()));

    // Ensure the ruleset is also loaded on the IO thread.
    content::RunAllTasksUntilIdle();

    // Ensure no load errors were reported.
    EXPECT_TRUE(ExtensionErrorReporter::GetInstance()->GetErrors()->empty());

    tester.ExpectTotalCount(kIndexRulesTimeHistogram, 1);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 1);
    tester.ExpectUniqueSample(kManifestRulesCountHistogram,
                              rules.size() /*sample*/, 1 /*count*/);
    tester.ExpectTotalCount(
        "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime", 1);
    tester.ExpectUniqueSample(
        "Extensions.DeclarativeNetRequest.LoadRulesetResult",
        RulesetMatcher::kLoadSuccess /*sample*/, 1 /*count*/);
  }

  void LoadExtensionWithRules(const std::vector<TestRule>& rules) {
    LoadExtensionWithRules(rules, "test_extension");
  }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestBrowserTest);
};

using DeclarativeNetRequestBrowserTest_Packed =
    DeclarativeNetRequestBrowserTest;

// Tests the "urlFilter" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_UrlFilter) {
  struct {
    std::string url_filter;
    int id;
  } rules_data[] = {
      {"pages_with_script/*ex", 1},
      {"||a.b.com", 2},
      {"|http://*.us", 3},
      {"pages_with_script/page2.html|", 4},
      {"|http://msn*/pages_with_script/page.html|", 5},
      {"pages_with_script/page2.html?q=bye^", 6},
      {"%20", 7},  // Block any urls with space.
  };

  // Rule |i| is the rule with id |i|.
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"example.com", "/pages_with_script/index.html", false},  // Rule 1
      {"example.com", "/pages_with_script/page.html", true},
      {"a.b.com", "/pages_with_script/page.html", false},    // Rule 2
      {"c.a.b.com", "/pages_with_script/page.html", false},  // Rule 2
      {"b.com", "/pages_with_script/page.html", true},
      {"example.us", "/pages_with_script/page.html", false},  // Rule 3
      {"example.jp", "/pages_with_script/page.html", true},
      {"example.jp", "/pages_with_script/page2.html", false},  // Rule 4
      {"example.jp", "/pages_with_script/page2.html?q=hello", true},
      {"msn.com", "/pages_with_script/page.html", false},  // Rule 5
      {"msn.com", "/pages_with_script/page.html?q=hello", true},
      {"a.msn.com", "/pages_with_script/page.html", true},

      // '^' (Separator character) matches anything except a letter, a digit or
      // one of the following: _ - . %.
      {"google.com", "/pages_with_script/page2.html?q=bye&x=1",
       false},  // Rule 6
      {"google.com", "/pages_with_script/page2.html?q=bye#x=1",
       false},                                                        // Rule 6
      {"google.com", "/pages_with_script/page2.html?q=bye:", false},  // Rule 6
      {"google.com", "/pages_with_script/page2.html?q=bye3", true},
      {"google.com", "/pages_with_script/page2.html?q=byea", true},
      {"google.com", "/pages_with_script/page2.html?q=bye_", true},
      {"google.com", "/pages_with_script/page2.html?q=bye-", true},
      {"google.com", "/pages_with_script/page2.html?q=bye%", true},
      {"google.com", "/pages_with_script/page2.html?q=bye.", true},

      {"abc.com", "/pages_with_script/page.html?q=hi bye", false},    // Rule 7
      {"abc.com", "/pages_with_script/page.html?q=hi%20bye", false},  // Rule 7
      {"abc.com", "/pages_with_script/page.html?q=hibye", true},
  };

  // Verify that all test urls load successfully without the DNR extension.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  }

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Verify that the extension correctly intercepts network requests.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that the separator character '^' correctly matches the end of the url.
// TODO(crbug.com/772260): Enable once the bug is fixed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       DISABLED_BlockRequests_SeparatorMatchesEndOfURL) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("page2.html^");

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  GURL url = embedded_test_server()->GetURL("google.com",
                                            "/pages_with_script/page2.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
}

// Tests case sensitive url filters.
// TODO(crbug.com/767605): Enable once case-insensitive matching is implemented
// in url_pattern_index.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       DISABLED_BlockRequests_IsUrlFilterCaseSensitive) {
  struct {
    std::string url_filter;
    size_t id;
    bool is_url_filter_case_sensitive;
  } rules_data[] = {{"pages_with_script/index.html?q=hello", 1, false},
                    {"pages_with_script/page.html?q=hello", 2, true}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.condition->is_url_filter_case_sensitive =
        rule_data.is_url_filter_case_sensitive;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Rule |i| is the rule with id |i|.
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"example.com", "/pages_with_script/index.html?q=hello",
       false},  // Rule 1
      {"example.com", "/pages_with_script/index.html?q=HELLO",
       false},                                                         // Rule 1
      {"example.com", "/pages_with_script/page.html?q=hello", false},  // Rule 2
      {"example.com", "/pages_with_script/page.html?q=HELLO", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests the "domains" and "excludedDomains" property of a declarative rule
// condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Domains) {
  struct {
    std::string url_filter;
    size_t id;
    std::vector<std::string> domains;
    std::vector<std::string> excluded_domains;
  } rules_data[] = {{"child_frame.html?frame=1", 1, {"x.com"}, {"a.x.com"}},
                    {"child_frame.html?frame=2", 2, {}, {"a.y.com"}}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;

    // An empty list is not allowed for the "domains" property.
    if (!rule_data.domains.empty())
      rule.condition->domains = rule_data.domains;

    rule.condition->excluded_domains = rule_data.excluded_domains;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Rule |i| is the rule with id |i|.
  struct {
    std::string main_frame_hostname;
    bool expect_frame_1_loaded;
    bool expect_frame_2_loaded;
  } test_cases[] = {
      {"x.com", false /* Rule 1 */, false /* Rule 2 */},
      {"b.x.com", false /* Rule 1 */, false /* Rule 2 */},
      {"a.x.com", true, false /* Rule 2 */},
      {"b.a.x.com", true, false /* Rule 2 */},
      {"y.com", true, false /* Rule 2*/},
      {"a.y.com", true, true},
      {"b.a.y.com", true, true},
      {"google.com", true, false /* Rule 2 */},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.main_frame_hostname,
                                              "/page_with_two_frames.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    content::RenderFrameHost* main_frame = GetMainFrame();
    EXPECT_TRUE(WasFrameWithScriptLoaded(main_frame));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

    content::RenderFrameHost* child_1 = content::ChildFrameAt(main_frame, 0);
    content::RenderFrameHost* child_2 = content::ChildFrameAt(main_frame, 1);

    EXPECT_EQ(test_case.expect_frame_1_loaded,
              WasFrameWithScriptLoaded(child_1));
    EXPECT_EQ(test_case.expect_frame_2_loaded,
              WasFrameWithScriptLoaded(child_2));
  }
}

// Tests the "domainType" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_DomainType) {
  struct {
    std::string url_filter;
    size_t id;
    base::Optional<std::string> domain_type;
  } rules_data[] = {
      {"child_frame.html?case=1", 1, std::string("firstParty")},
      {"child_frame.html?case=2", 2, std::string("thirdParty")},
      {"child_frame.html?case=3", 3, base::nullopt},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.condition->domain_type = rule_data.domain_type;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  GURL url =
      embedded_test_server()->GetURL("example.com", "/domain_type_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

  // The loaded page will consist of four pairs of iframes named
  // first_party_[1..4] and third_party_[1..4], with url path
  // child_frame.html?case=[1..4] respectively.
  struct {
    std::string child_frame_name;
    bool expect_frame_loaded;
  } cases[] = {
      // Url matched by rule 1. Only the first party frame is blocked.
      {"first_party_1", false},
      {"third_party_1", true},
      // Url matched by rule 2. Only the third party frame is blocked.
      {"first_party_2", true},
      {"third_party_2", false},
      // Url matched by rule 3. Both the first and third party frames are
      // blocked.
      {"first_party_3", false},
      {"third_party_3", false},
      // No matching rule.
      {"first_party_4", true},
      {"third_party_4", true},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing child frame named %s",
                                    test_case.child_frame_name.c_str()));

    content::RenderFrameHost* child =
        GetFrameByName(test_case.child_frame_name);
    EXPECT_TRUE(child);
    EXPECT_EQ(test_case.expect_frame_loaded, WasFrameWithScriptLoaded(child));
  }
}

// Tests whitelisting rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, Whitelist) {
  const int kNumRequests = 6;

  TestRule rule = CreateGenericRule();
  int id = kMinValidID;

  // Block all requests ending with numbers 1 to |kNumRequests|.
  std::vector<TestRule> rules;
  for (int i = 1; i <= kNumRequests; ++i) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rules.push_back(rule);
  }

  // Whitelist all requests ending with even numbers from 1 to |kNumRequests|.
  for (int i = 2; i <= kNumRequests; i += 2) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rule.action->type = std::string("whitelist");
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  for (int i = 1; i <= kNumRequests; ++i) {
    GURL url = embedded_test_server()->GetURL(
        "example.com",
        base::StringPrintf("/pages_with_script/page.html?num=%d", i));
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    // All requests ending with odd numbers should be blocked.
    const bool page_should_load = (i % 2 == 0);
    EXPECT_EQ(page_should_load, WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type =
        page_should_load ? content::PAGE_TYPE_NORMAL : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that the extension ruleset is active only when the extension is
// enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       Enable_Disable_Reload_Uninstall) {
  // Block all requests to example.com
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId extension_id = last_loaded_extension_id();

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  auto test_extension_enabled = [&](bool expected_enabled) {
    // Wait for any pending actions caused by extension state change.
    content::RunAllTasksUntilIdle();
    EXPECT_EQ(expected_enabled,
              ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
                  extension_id));

    ui_test_utils::NavigateToURL(browser(), url);

    // If the extension is enabled, the |url| should be blocked.
    EXPECT_EQ(!expected_enabled, WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type =
        expected_enabled ? content::PAGE_TYPE_ERROR : content::PAGE_TYPE_NORMAL;
    EXPECT_EQ(expected_page_type, GetPageType());
  };

  {
    SCOPED_TRACE("Testing extension after load");
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing DisableExtension");
    DisableExtension(extension_id);
    test_extension_enabled(false);
  }

  {
    SCOPED_TRACE("Testing EnableExtension");
    EnableExtension(extension_id);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing ReloadExtension");
    ReloadExtension(extension_id);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing UninstallExtension");
    UninstallExtension(extension_id);
    test_extension_enabled(false);
  }
}

// Tests that multiple enabled extensions with declarative rulesets having
// blocking rules behave correctly.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_MultipleExtensions) {
  struct {
    std::string url_filter;
    int id;
    bool add_to_first_extension;
    bool add_to_second_extension;
  } rules_data[] = {
      {"block_both.com", 1, true, true},
      {"block_first.com", 2, true, false},
      {"block_second.com", 3, false, true},
  };

  std::vector<TestRule> rules_1, rules_2;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    if (rule_data.add_to_first_extension)
      rules_1.push_back(rule);
    if (rule_data.add_to_second_extension)
      rules_2.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules_1, "extension_1"));
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules_2, "extension_2"));

  struct {
    std::string host;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"block_both.com", false},
      {"block_first.com", false},
      {"block_second.com", false},
      {"block_none.com", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.host,
                                              "/pages_with_script/page.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that only extensions enabled in incognito mode affect network requests
// from an incognito context.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Incognito) {
  // Block all requests to example.com
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  ExtensionId extension_id = last_loaded_extension_id();
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  Browser* incognito_browser = CreateIncognitoBrowser();

  auto test_enabled_in_incognito = [&](bool expected_enabled_in_incognito) {
    // Wait for any pending actions caused by extension state change.
    content::RunAllTasksUntilIdle();

    EXPECT_EQ(expected_enabled_in_incognito,
              util::IsIncognitoEnabled(extension_id, profile()));

    // The url should be blocked in normal context.
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
    EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());

    // In incognito context, the url should be blocked if the extension is
    // enabled in incognito mode.
    ui_test_utils::NavigateToURL(incognito_browser, url);
    EXPECT_EQ(!expected_enabled_in_incognito,
              WasFrameWithScriptLoaded(GetMainFrame(incognito_browser)));
    content::PageType expected_page_type = expected_enabled_in_incognito
                                               ? content::PAGE_TYPE_ERROR
                                               : content::PAGE_TYPE_NORMAL;
    EXPECT_EQ(expected_page_type, GetPageType(incognito_browser));
  };

  {
    // By default, the extension will be disabled in incognito.
    SCOPED_TRACE("Testing extension after load");
    test_enabled_in_incognito(false);
  }

  {
    // Enable the extension in incognito mode.
    SCOPED_TRACE("Testing extension after enabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), true /*enabled*/);
    test_enabled_in_incognito(true);
  }

  // Disable the extension in incognito mode.
  {
    SCOPED_TRACE("Testing extension after disabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), false /*enabled*/);
    test_enabled_in_incognito(false);
  }
}

// Tests the "resourceTypes" and "excludedResourceTypes" fields of a declarative
// rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_ResourceTypes) {
  // TODO(crbug.com/696822): Add tests for "object", "ping", "other", "font".
  enum ResourceTypeMask {
    kNone = 0,
    kSubframe = 1 << 0,
    kStylesheet = 1 << 1,
    kScript = 1 << 2,
    kImage = 1 << 3,
    kXHR = 1 << 4,
    kMedia = 1 << 5,
    kWebSocket = 1 << 6,
    kAll = (1 << 7) - 1
  };

  struct {
    std::string domain;
    size_t id;
    std::vector<std::string> resource_types;
    std::vector<std::string> excluded_resource_types;
  } rules_data[] = {
      {"block_subframe.com", 1, {"sub_frame"}, {}},
      {"block_stylesheet.com", 2, {"stylesheet"}, {}},
      {"block_script.com", 3, {"script"}, {}},
      {"block_image.com", 4, {"image"}, {}},
      {"block_xhr.com", 5, {"xmlhttprequest"}, {}},
      {"block_media.com", 6, {"media"}, {}},
      {"block_websocket.com", 7, {"websocket"}, {}},
      {"block_image_and_stylesheet.com", 8, {"image", "stylesheet"}, {}},
      {"block_subframe_and_xhr.com", 11, {"sub_frame", "xmlhttprequest"}, {}},
      // With renderer side navigation, the main frame origin serves as the
      // initiator for main frame page loads. Hence to ensure that the main
      // frame page load is not blocked, also exclude the "other" resource type,
      // which is used for main frame requests currently.
      // TODO(crbug.com/696822): Change "other" to "main_frame" once it is
      // implemented.
      {"block_all.com", 9, {}, {"other"}},
      {"block_all_but_xhr_and_script.com",
       10,
       {},
       {"xmlhttprequest", "script", "other"}},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();

    // The "resourceTypes" property (i.e. |rule.condition->resource_types|)
    // should not be an empty list. It should either be omitted or be a non-
    // empty list.
    if (rule_data.resource_types.empty())
      rule.condition->resource_types = base::nullopt;
    else
      rule.condition->resource_types = rule_data.resource_types;

    rule.condition->excluded_resource_types = rule_data.excluded_resource_types;
    rule.id = rule_data.id;
    rule.condition->domains = std::vector<std::string>({rule_data.domain});
    // Don't specify the urlFilter, which should behaves the same as "*".
    rule.condition->url_filter = base::nullopt;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  struct {
    std::string hostname;
    int blocked_mask;
  } test_cases[] = {
      {"block_subframe.com", kSubframe},
      {"block_stylesheet.com", kStylesheet},
      {"block_script.com", kScript},
      {"block_image.com", kImage},
      {"block_xhr.com", kXHR},
      {"block_media.com", kMedia},
      {"block_websocket.com", kWebSocket},
      {"block_image_and_stylesheet.com", kImage | kStylesheet},
      {"block_subframe_and_xhr.com", kSubframe | kXHR},
      {"block_all.com", kAll},
      {"block_all_but_xhr_and_script.com", kAll & ~kXHR & ~kScript},
      {"block_none.com", kNone}};

  // Start a web socket test server to test the websocket resource type.
  net::SpawnedTestServer websocket_test_server(
      net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(websocket_test_server.Start());

  // The |websocket_url| will echo the message we send to it.
  GURL websocket_url = websocket_test_server.GetURL("echo-with-no-extension");

  auto execute_script = [](content::RenderFrameHost* frame,
                           const std::string& script) {
    bool subresource_loaded = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(frame, script,
                                                     &subresource_loaded));
    return subresource_loaded;
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.hostname,
                                              "/subresources.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

    content::RenderFrameHost* frame = GetMainFrame();

    // sub-frame.
    EXPECT_EQ(
        !(test_case.blocked_mask & kSubframe),
        execute_script(frame,
                       "domAutomationController.send(!!window.frameLoaded);"));

    // stylesheet
    EXPECT_EQ(!(test_case.blocked_mask & kStylesheet),
              execute_script(frame, "testStylesheet();"));

    // script
    EXPECT_EQ(!(test_case.blocked_mask & kScript),
              execute_script(frame, "testScript();"));

    // image
    EXPECT_EQ(!(test_case.blocked_mask & kImage),
              execute_script(frame, "testImage();"));

    // xhr
    EXPECT_EQ(!(test_case.blocked_mask & kXHR),
              execute_script(frame, "testXHR();"));

    // media
    EXPECT_EQ(!(test_case.blocked_mask & kMedia),
              execute_script(frame, "testMedia();"));

    // websocket
    EXPECT_EQ(!(test_case.blocked_mask & kWebSocket),
              execute_script(frame,
                             base::StringPrintf("testWebSocket('%s');",
                                                websocket_url.spec().c_str())));
  }
}

// Ensure extensions can't intercept chrome:// urls.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ChromeURLS) {
  // Have the extension block all chrome:// urls.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("chrome://");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  std::vector<const char*> test_urls = {
      chrome::kChromeUIFlagsURL, chrome::kChromeUIAboutURL,
      chrome::kChromeUIExtensionsURL, chrome::kChromeUIVersionURL};

  for (const char* url : test_urls) {
    ui_test_utils::NavigateToURL(browser(), GURL(url));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  }
}

// Test that a packed extension with a DNR ruleset behaves correctly after
// browser restart.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PRE_BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  // Ensure that the DNR extension enabled in previous browser session still
  // correctly blocks network requests.
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
}

// Ensure that Blink's in-memory cache is cleared on adding/removing rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RendererCacheCleared) {
  // Set-up an observer for RulesetMatcher to monitor requests to
  // script.js.
  URLRequestMonitor script_monitor(
      embedded_test_server()->GetURL("example.com", "/cached/script.js"));
  scoped_refptr<InfoMap> info_map =
      base::WrapRefCounted(ExtensionSystem::Get(profile())->info_map());
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetRulesetManagerObserverOnIOThread, &script_monitor,
                     info_map));
  content::RunAllTasksUntilIdle();

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/cached/page_with_cacheable_script.html");

  // With no extension loaded, the request to the script should succeed.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(script_monitor.GetAndResetRequestSeen(false));

  // Another request to |url| should not cause a network request for
  // script.js since it will be served by the renderer's in-memory
  // cache.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_FALSE(script_monitor.GetAndResetRequestSeen(false));

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Adding an extension ruleset should have cleared the renderer's in-memory
  // cache. Hence the browser process will observe the request to
  // script.js and block it.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(script_monitor.GetAndResetRequestSeen(false));

  // Disable the extension and wait for the ruleset to be unloaded on the IO
  // thread.
  DisableExtension(last_loaded_extension_id());
  content::RunAllTasksUntilIdle();

  // Disabling the extension should cause the request to succeed again. The
  // request for the script will again be observed by the browser since it's not
  // in the renderer's in-memory cache.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(script_monitor.GetAndResetRequestSeen(false));

  // Clear RulesetManager's observer.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetRulesetManagerObserverOnIOThread, nullptr, info_map));
  content::RunAllTasksUntilIdle();
}

INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestBrowserTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestBrowserTest_Packed,
                        ::testing::Values(ExtensionLoadType::PACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
