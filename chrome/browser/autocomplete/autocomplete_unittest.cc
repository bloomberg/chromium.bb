// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

static std::ostream& operator<<(std::ostream& os,
                                const AutocompleteResult::const_iterator& it) {
  return os << static_cast<const AutocompleteMatch*>(&(*it));
}

namespace {

const size_t num_results_per_provider = 3;

// Autocomplete provider that provides known results. Note that this is
// refcounted so that it can also be a task on the message loop.
class TestProvider : public AutocompleteProvider {
 public:
  TestProvider(int relevance, const string16& prefix)
      : AutocompleteProvider(NULL, NULL, ""),
        relevance_(relevance),
        prefix_(prefix) {
  }

  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes);

  void set_listener(ACProviderListener* listener) {
    listener_ = listener;
  }

 private:
  ~TestProvider() {}

  void Run();

  void AddResults(int start_at, int num);

  int relevance_;
  const string16 prefix_;
};

void TestProvider::Start(const AutocompleteInput& input,
                         bool minimal_changes) {
  if (minimal_changes)
    return;

  matches_.clear();

  // Generate one result synchronously, the rest later.
  AddResults(0, 1);

  if (!input.synchronous_only()) {
    done_ = false;
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TestProvider::Run));
  }
}

void TestProvider::Run() {
  DCHECK_GT(num_results_per_provider, 0U);
  AddResults(1, num_results_per_provider);
  done_ = true;
  DCHECK(listener_);
  listener_->OnProviderUpdate(true);
}

void TestProvider::AddResults(int start_at, int num) {
  for (int i = start_at; i < num; i++) {
    AutocompleteMatch match(this, relevance_ - i, false,
                            AutocompleteMatch::URL_WHAT_YOU_TYPED);

    match.fill_into_edit = prefix_ + UTF8ToUTF16(base::IntToString(i));
    match.destination_url = GURL(UTF16ToUTF8(match.fill_into_edit));

    match.contents = match.fill_into_edit;
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.description = match.fill_into_edit;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));

    matches_.push_back(match);
  }
}

class AutocompleteProviderTest : public testing::Test,
                                 public NotificationObserver {
 protected:
  void ResetControllerWithTestProviders(bool same_destinations);

  // Runs a query on the input "a", and makes sure both providers' input is
  // properly collected.
  void RunTest();

  void ResetControllerWithTestProvidersWithKeywordAndSearchProviders();
  void RunExactKeymatchTest(bool allow_exact_keyword_match);

  // These providers are owned by the controller once it's created.
  ACProviders providers_;

  AutocompleteResult result_;

 private:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  ScopedTestingBrowserProcess browser_process_;

  MessageLoopForUI message_loop_;
  scoped_ptr<AutocompleteController> controller_;
  NotificationRegistrar registrar_;
  TestingProfile profile_;
};

void AutocompleteProviderTest::ResetControllerWithTestProviders(
    bool same_destinations) {
  // Forget about any existing providers.  The controller owns them and will
  // Release() them below, when we delete it during the call to reset().
  providers_.clear();

  // Construct two new providers, with either the same or different prefixes.
  TestProvider* providerA = new TestProvider(num_results_per_provider,
                                             ASCIIToUTF16("http://a"));
  providerA->AddRef();
  providers_.push_back(providerA);

  TestProvider* providerB = new TestProvider(num_results_per_provider * 2,
      same_destinations ? ASCIIToUTF16("http://a") : ASCIIToUTF16("http://b"));
  providerB->AddRef();
  providers_.push_back(providerB);

  // Reset the controller to contain our new providers.
  AutocompleteController* controller = new AutocompleteController(providers_);
  controller_.reset(controller);
  providerA->set_listener(controller);
  providerB->set_listener(controller);

  // The providers don't complete synchronously, so listen for "result updated"
  // notifications.
  registrar_.Add(this, NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_READY,
                 NotificationService::AllSources());
}

void AutocompleteProviderTest::
    ResetControllerWithTestProvidersWithKeywordAndSearchProviders() {
  profile_.CreateTemplateURLModel();

  // Reset the default TemplateURL.
  TemplateURL* default_t_url = new TemplateURL();
  default_t_url->SetURL("http://defaultturl/{searchTerms}", 0, 0);
  TemplateURLModel* turl_model = profile_.GetTemplateURLModel();
  turl_model->Add(default_t_url);
  turl_model->SetDefaultSearchProvider(default_t_url);
  TemplateURLID default_provider_id = default_t_url->id();
  ASSERT_NE(0, default_provider_id);

  // Create another TemplateURL for KeywordProvider.
  TemplateURL* keyword_t_url = new TemplateURL();
  keyword_t_url->set_short_name(ASCIIToUTF16("k"));
  keyword_t_url->set_keyword(ASCIIToUTF16("k"));
  keyword_t_url->SetURL("http://keyword/{searchTerms}", 0, 0);
  profile_.GetTemplateURLModel()->Add(keyword_t_url);
  ASSERT_NE(0, keyword_t_url->id());

  // Forget about any existing providers.  The controller owns them and will
  // Release() them below, when we delete it during the call to reset().
  providers_.clear();

  // Create both a keyword and search provider, and add them in that order.
  // (Order is important; see comments in RunExactKeymatchTest().)
  AutocompleteProvider* keyword_provider = new KeywordProvider(NULL,
                                                                &profile_);
  keyword_provider->AddRef();
  providers_.push_back(keyword_provider);
  AutocompleteProvider* search_provider = new SearchProvider(NULL, &profile_);
  search_provider->AddRef();
  providers_.push_back(search_provider);

  AutocompleteController* controller = new AutocompleteController(providers_);
  controller_.reset(controller);
}

void AutocompleteProviderTest::RunTest() {
  result_.Reset();
  controller_->Start(ASCIIToUTF16("a"), string16(), true, false, true, false);

  // The message loop will terminate when all autocomplete input has been
  // collected.
  MessageLoop::current()->Run();
}

void AutocompleteProviderTest::RunExactKeymatchTest(
    bool allow_exact_keyword_match) {
  // Send the controller input which exactly matches the keyword provider we
  // created in ResetControllerWithKeywordAndSearchProviders().  The default
  // match should thus be a keyword match iff |allow_exact_keyword_match| is
  // true.
  controller_->Start(ASCIIToUTF16("k test"), string16(), true, false,
                     allow_exact_keyword_match, true);
  EXPECT_TRUE(controller_->done());
  // ResetControllerWithKeywordAndSearchProviders() adds the keyword provider
  // first, then the search provider.  So if the default match is a keyword
  // match, it will come from provider 0, otherwise from provider 1.
  EXPECT_EQ(providers_[allow_exact_keyword_match ? 0 : 1],
      controller_->result().default_match()->provider);
}

void AutocompleteProviderTest::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (controller_->done()) {
    result_.CopyFrom(controller_->result());
    MessageLoop::current()->Quit();
  }
}

// Tests that the default selection is set properly when updating results.
TEST_F(AutocompleteProviderTest, Query) {
  ResetControllerWithTestProviders(false);
  RunTest();

  // Make sure the default match gets set to the highest relevance match.  The
  // highest relevance matches should come from the second provider.
  EXPECT_EQ(num_results_per_provider * 2, result_.size());  // two providers
  ASSERT_NE(result_.end(), result_.default_match());
  EXPECT_EQ(providers_[1], result_.default_match()->provider);
}

TEST_F(AutocompleteProviderTest, RemoveDuplicates) {
  ResetControllerWithTestProviders(true);
  RunTest();

  // Make sure all the first provider's results were eliminated by the second
  // provider's.
  EXPECT_EQ(num_results_per_provider, result_.size());
  for (AutocompleteResult::const_iterator i(result_.begin());
       i != result_.end(); ++i)
    EXPECT_EQ(providers_[1], i->provider);
}

TEST_F(AutocompleteProviderTest, AllowExactKeywordMatch) {
  ResetControllerWithTestProvidersWithKeywordAndSearchProviders();
  RunExactKeymatchTest(true);
  RunExactKeymatchTest(false);
}

typedef TestingBrowserProcessTest AutocompleteTest;

TEST_F(AutocompleteTest, InputType) {
  struct test_data {
    const string16 input;
    const AutocompleteInput::Type type;
  } input_cases[] = {
    { ASCIIToUTF16(""), AutocompleteInput::INVALID },
    { ASCIIToUTF16("?"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("?foo"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("?foo bar"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("?http://foo.com/bar"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("foo"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo.c"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("-.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo/bar"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo;bar"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo/bar baz"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo bar.com"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo bar"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo+bar"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo+bar.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("\"foo:bar\""), AutocompleteInput::QUERY },
    { ASCIIToUTF16("link:foo.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("www.foo.com:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("localhost:8080"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo.com:123456"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo.com:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("1.2.3.4:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("user@foo.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user:pass@"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user:pass@!foo.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user:pass@foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo.c"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo.com:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("1.2"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("1.2/45"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("1.2:45"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user@1.2:45"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user:pass@1.2:45"), AutocompleteInput::URL },
    { ASCIIToUTF16("ps/2 games"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("en.wikipedia.org/wiki/James Bond"),
        AutocompleteInput::URL },
    // In Chrome itself, mailto: will get handled by ShellExecute, but in
    // unittest mode, we don't have the data loaded in the external protocol
    // handler to know this.
    // { ASCIIToUTF16("mailto:abuse@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("view-source:http://www.foo.com/"), AutocompleteInput::URL },
    { ASCIIToUTF16("javascript:alert(\"Hey there!\");"),
        AutocompleteInput::URL },
#if defined(OS_WIN)
    { ASCIIToUTF16("C:\\Program Files"), AutocompleteInput::URL },
    { ASCIIToUTF16("\\\\Server\\Folder\\File"), AutocompleteInput::URL },
#endif  // defined(OS_WIN)
    { ASCIIToUTF16("http:foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo.c"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo_bar.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo/bar baz"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://-.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("http://_foo_.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("http://foo.com:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("http://foo.com:123456"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("http://1.2.3.4:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("http:user@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://user@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http:user:pass@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://user:pass@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://1.2"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://1.2/45"), AutocompleteInput::URL },
    { ASCIIToUTF16("http:ps/2 games"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://ps/2 games"), AutocompleteInput::URL },
    { ASCIIToUTF16("https://foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("127.0.0.1"), AutocompleteInput::URL },
    { ASCIIToUTF16("127.0.1"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("127.0.1/"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("browser.tabs.closeButtons"), AutocompleteInput::UNKNOWN },
    { WideToUTF16(L"\u6d4b\u8bd5"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("[2001:]"), AutocompleteInput::QUERY },  // Not a valid IP
    { ASCIIToUTF16("[2001:dB8::1]"), AutocompleteInput::URL },
    { ASCIIToUTF16("192.168.0.256"),
        AutocompleteInput::QUERY },  // Invalid IPv4 literal.
    { ASCIIToUTF16("[foo.com]"),
        AutocompleteInput::QUERY },  // Invalid IPv6 literal.
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input, string16(), true, false,
                            true, false);
    EXPECT_EQ(input_cases[i].type, input.type());
  }
}

TEST_F(AutocompleteTest, InputTypeWithDesiredTLD) {
  struct test_data {
    const string16 input;
    const AutocompleteInput::Type type;
  } input_cases[] = {
    { ASCIIToUTF16("401k"), AutocompleteInput::REQUESTED_URL },
    { ASCIIToUTF16("999999999999999"), AutocompleteInput::REQUESTED_URL },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    AutocompleteInput input(input_cases[i].input, ASCIIToUTF16("com"), true,
                            false, true, false);
    EXPECT_EQ(input_cases[i].type, input.type()) << "Input: " <<
        input_cases[i].input;
  }
}

// This tests for a regression where certain input in the omnibox caused us to
// crash. As long as the test completes without crashing, we're fine.
TEST_F(AutocompleteTest, InputCrash) {
  AutocompleteInput input(WideToUTF16(L"\uff65@s"), string16(), true, false,
                          true, false);
}

// Test comparing matches relevance.
TEST(AutocompleteMatch, MoreRelevant) {
  struct RelevantCases {
    int r1;
    int r2;
    bool expected_result;
  } cases[] = {
    {  10,   0, true  },
    {  10,  -5, true  },
    {  -5,  10, false },
    {   0,  10, false },
    { -10,  -5, false  },
    {  -5, -10, true },
  };

  AutocompleteMatch m1(NULL, 0, false, AutocompleteMatch::URL_WHAT_YOU_TYPED);
  AutocompleteMatch m2(NULL, 0, false, AutocompleteMatch::URL_WHAT_YOU_TYPED);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    m1.relevance = cases[i].r1;
    m2.relevance = cases[i].r2;
    EXPECT_EQ(cases[i].expected_result,
              AutocompleteMatch::MoreRelevant(m1, m2));
  }
}

TEST(AutocompleteInput, ParseForEmphasizeComponent) {
  using url_parse::Component;
  Component kInvalidComponent(0, -1);
  struct test_data {
    const string16 input;
    const Component scheme;
    const Component host;
  } input_cases[] = {
    { ASCIIToUTF16(""), kInvalidComponent, kInvalidComponent },
    { ASCIIToUTF16("?"), kInvalidComponent, kInvalidComponent },
    { ASCIIToUTF16("?http://foo.com/bar"), kInvalidComponent,
        kInvalidComponent },
    { ASCIIToUTF16("foo/bar baz"), kInvalidComponent, Component(0, 3) },
    { ASCIIToUTF16("http://foo/bar baz"), Component(0, 4), Component(7, 3) },
    { ASCIIToUTF16("link:foo.com"), Component(0, 4), kInvalidComponent },
    { ASCIIToUTF16("www.foo.com:81"), kInvalidComponent, Component(0, 11) },
    { WideToUTF16(L"\u6d4b\u8bd5"), kInvalidComponent, Component(0, 2) },
    { ASCIIToUTF16("view-source:http://www.foo.com/"), Component(12, 4),
        Component(19, 11) },
    { ASCIIToUTF16("view-source:https://example.com/"),
      Component(12, 5), Component(20, 11) },
    { ASCIIToUTF16("view-source:www.foo.com"), kInvalidComponent,
        Component(12, 11) },
    { ASCIIToUTF16("view-source:"), Component(0, 11), kInvalidComponent },
    { ASCIIToUTF16("view-source:garbage"), kInvalidComponent,
        Component(12, 7) },
    { ASCIIToUTF16("view-source:http://http://foo"), Component(12, 4),
        Component(19, 4) },
    { ASCIIToUTF16("view-source:view-source:http://example.com/"),
        Component(12, 11), kInvalidComponent }
  };

  ScopedTestingBrowserProcess browser_process;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    Component scheme, host;
    AutocompleteInput::ParseForEmphasizeComponents(input_cases[i].input,
                                                   string16(),
                                                   &scheme,
                                                   &host);
    AutocompleteInput input(input_cases[i].input, string16(), true, false,
                            true, false);
    EXPECT_EQ(input_cases[i].scheme.begin, scheme.begin) << "Input: " <<
        input_cases[i].input;
    EXPECT_EQ(input_cases[i].scheme.len, scheme.len) << "Input: " <<
        input_cases[i].input;
    EXPECT_EQ(input_cases[i].host.begin, host.begin) << "Input: " <<
        input_cases[i].input;
    EXPECT_EQ(input_cases[i].host.len, host.len) << "Input: " <<
        input_cases[i].input;
  }
}

}  // namespace
