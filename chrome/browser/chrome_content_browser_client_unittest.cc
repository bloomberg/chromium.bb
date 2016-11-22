// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <list>
#include <map>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/origin_filter_builder.h"
#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/media_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#endif

using testing::_;
using ChromeContentBrowserClientTest = testing::Test;

TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.ShouldAssignSiteForURL(GURL("chrome-native://test")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("http://www.google.com")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("https://www.google.com")));
}

// BrowserWithTestWindowTest doesn't work on Android.
#if !defined(OS_ANDROID)

using ChromeContentBrowserClientWindowTest = BrowserWithTestWindowTest;

static void DidOpenURLForWindowTest(content::WebContents** target_contents,
                                    content::WebContents* opened_contents) {
  DCHECK(target_contents);

  *target_contents = opened_contents;
}

TEST_F(ChromeContentBrowserClientWindowTest, IsDataSaverEnabled) {
  ChromeContentBrowserClient client;
  content::BrowserContext* context = browser()->profile();
  EXPECT_FALSE(client.IsDataSaverEnabled(context));
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDataSaverEnabled, true);
  EXPECT_TRUE(client.IsDataSaverEnabled(context));
}

// This test opens two URLs using ContentBrowserClient::OpenURL. It expects the
// URLs to be opened in new tabs and activated, changing the active tabs after
// each call and increasing the tab count by 2.
TEST_F(ChromeContentBrowserClientWindowTest, OpenURL) {
  ChromeContentBrowserClient client;

  int previous_count = browser()->tab_strip_model()->count();

  GURL urls[] = { GURL("https://www.google.com"),
                  GURL("https://www.chromium.org") };

  for (const GURL& url : urls) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    // TODO(peter): We should have more in-depth browser tests for the window
    // opening functionality, which also covers Android. This test can currently
    // only be ran on platforms where OpenURL is implemented synchronously.
    // See https://crbug.com/457667.
    content::WebContents* web_contents = nullptr;
    client.OpenURL(browser()->profile(),
                   params,
                   base::Bind(&DidOpenURLForWindowTest, &web_contents));

    EXPECT_TRUE(web_contents);

    content::WebContents* active_contents = browser()->tab_strip_model()->
        GetActiveWebContents();
    EXPECT_EQ(web_contents, active_contents);
    EXPECT_EQ(url, active_contents->GetVisibleURL());
  }

  EXPECT_EQ(previous_count + 2, browser()->tab_strip_model()->count());
}

#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_WEBRTC)

// NOTE: Any updates to the expectations in these tests should also be done in
// the browser test WebRtcDisableEncryptionFlagBrowserTest.
class DisableWebRtcEncryptionFlagTest : public testing::Test {
 public:
  DisableWebRtcEncryptionFlagTest()
      : from_command_line_(base::CommandLine::NO_PROGRAM),
        to_command_line_(base::CommandLine::NO_PROGRAM) {}

 protected:
  void SetUp() override {
    from_command_line_.AppendSwitch(switches::kDisableWebRtcEncryption);
  }

  void MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel channel) {
    ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
        &to_command_line_,
        from_command_line_,
        channel);
  }

  base::CommandLine from_command_line_;
  base::CommandLine to_command_line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisableWebRtcEncryptionFlagTest);
};

TEST_F(DisableWebRtcEncryptionFlagTest, UnknownChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::UNKNOWN);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, CanaryChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::CANARY);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, DevChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::DEV);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, BetaChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::BETA);
#if defined(OS_ANDROID)
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#else
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#endif
}

TEST_F(DisableWebRtcEncryptionFlagTest, StableChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::STABLE);
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

#endif  // ENABLE_WEBRTC

class BlinkSettingsFieldTrialTest : public testing::Test {
 public:
  static const char kParserFieldTrialName[];
  static const char kIFrameFieldTrialName[];
  static const char kFakeGroupName[];
  static const char kDefaultGroupName[];

  BlinkSettingsFieldTrialTest()
      : trial_list_(NULL),
        command_line_(base::CommandLine::NO_PROGRAM) {}

  void SetUp() override {
    command_line_.AppendSwitchASCII(
        switches::kProcessType, switches::kRendererProcess);
  }

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

  void CreateFieldTrial(const char* trial_name, const char* group_name) {
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  }

  void CreateFieldTrialWithParams(
      const char* trial_name,
      const char* group_name,
      const char* key1, const char* value1,
      const char* key2, const char* value2) {
    std::map<std::string, std::string> params;
    params.insert(std::make_pair(key1, value1));
    params.insert(std::make_pair(key2, value2));
    CreateFieldTrial(trial_name, kFakeGroupName);
    variations::AssociateVariationParams(trial_name, kFakeGroupName, params);
  }

  void AppendContentBrowserClientSwitches() {
    client_.AppendExtraCommandLineSwitches(&command_line_, kFakeChildProcessId);
  }

  const base::CommandLine& command_line() const {
    return command_line_;
  }

  void AppendBlinkSettingsSwitch(const char* value) {
    command_line_.AppendSwitchASCII(switches::kBlinkSettings, value);
  }

 private:
  static const int kFakeChildProcessId = 1;

  ChromeContentBrowserClient client_;
  base::FieldTrialList trial_list_;
  base::CommandLine command_line_;

  content::TestBrowserThreadBundle thread_bundle_;
};

const char BlinkSettingsFieldTrialTest::kParserFieldTrialName[] =
    "BackgroundHtmlParserTokenLimits";
const char BlinkSettingsFieldTrialTest::kIFrameFieldTrialName[] =
    "LowPriorityIFrames";
const char BlinkSettingsFieldTrialTest::kFakeGroupName[] = "FakeGroup";
const char BlinkSettingsFieldTrialTest::kDefaultGroupName[] = "Default";

TEST_F(BlinkSettingsFieldTrialTest, NoFieldTrial) {
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialWithoutParams) {
  CreateFieldTrial(kParserFieldTrialName, kFakeGroupName);
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, BlinkSettingsSwitchAlreadySpecified) {
  AppendBlinkSettingsSwitch("foo");
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("foo",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialEnabled) {
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, MultipleFieldTrialsEnabled) {
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  CreateFieldTrialWithParams(kIFrameFieldTrialName, kFakeGroupName,
                             "keyA", "valueA", "keyB", "valueB");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2,keyA=valueA,keyB=valueB",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, MultipleFieldTrialsDuplicateKeys) {
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  CreateFieldTrialWithParams(kIFrameFieldTrialName, kFakeGroupName,
                             "key2", "duplicate", "key3", "value3");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2,key2=duplicate,key3=value3",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

#if !defined(OS_ANDROID)
namespace content {

class InstantNTPURLRewriteTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<metrics::SHA1EntropyProvider>("42")));
  }

  void InstallTemplateURLWithNewTabPage(GURL new_tab_page_url) {
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.new_tab_url = new_tab_page_url.spec();
    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(InstantNTPURLRewriteTest, UberURLHandler_InstantExtendedNewTabPage) {
  const GURL url_original("chrome://newtab");
  const GURL url_rewritten("https://www.example.com/newtab");
  InstallTemplateURLWithNewTabPage(url_rewritten);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL("chrome://blank"));
  NavigateAndCommitActiveTab(url_original);

  NavigationEntry* entry = browser()->tab_strip_model()->
      GetActiveWebContents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_rewritten, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

}  // namespace content
#endif  // !defined(OS_ANDROID)

namespace {

// A BrowsingDataRemover that only records calls.
class MockBrowsingDataRemover : public BrowsingDataRemover {
 public:
  explicit MockBrowsingDataRemover(content::BrowserContext* context)
      : BrowsingDataRemover(context) {}

  ~MockBrowsingDataRemover() override {
    DCHECK(!expected_calls_.size())
        << "Expectations were set but not verified.";
  }

  void RemoveInternal(const TimeRange& time_range,
                      int remove_mask,
                      int origin_type_mask,
                      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                      BrowsingDataRemover::Observer* observer) override {
    actual_calls_.emplace_back(time_range, remove_mask, origin_type_mask,
                               std::move(filter_builder), UNKNOWN);

    // |observer| is not recorded in |actual_calls_| to be compared with
    // expectations, because it's created internally in ClearSiteData() and
    // it's unknown to this. However, it is tested implicitly, because we use
    // it for the completion callback, so an incorrect |observer| will fail
    // the test by waiting for the callback forever.
    DCHECK(observer);
    observer->OnBrowsingDataRemoverDone();
  }

  void ExpectCall(
      const TimeRange& time_range,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<RegistrableDomainFilterBuilder> filter_builder) {
    expected_calls_.emplace_back(time_range, remove_mask, origin_type_mask,
                                 std::move(filter_builder),
                                 REGISTRABLE_DOMAIN_FILTER_BUILDER);
  }

  void ExpectCall(const TimeRange& time_range,
                  int remove_mask,
                  int origin_type_mask,
                  std::unique_ptr<OriginFilterBuilder> filter_builder) {
    expected_calls_.emplace_back(time_range, remove_mask, origin_type_mask,
                                 std::move(filter_builder),
                                 ORIGIN_FILTER_BUILDER);
  }

  void ExpectCallDontCareAboutFilterBuilder(const TimeRange& time_range,
                                            int remove_mask,
                                            int origin_type_mask) {
    expected_calls_.emplace_back(time_range, remove_mask, origin_type_mask,
                                 std::unique_ptr<BrowsingDataFilterBuilder>(),
                                 DONT_CARE);
  }

  void VerifyAndClearExpectations() {
    EXPECT_EQ(expected_calls_, actual_calls_);
    expected_calls_.clear();
    actual_calls_.clear();
  }

 private:
  // Used to further specify the type and intention behind the passed
  // std::unique_ptr<BrowsingDataFilterBuilder>. This is needed for comparison
  // between the expected and actual call parameters.
  enum FilterBuilderType {
    REGISTRABLE_DOMAIN_FILTER_BUILDER,  // RegistrableDomainFilterBuilder
    ORIGIN_FILTER_BUILDER,              // OriginFilterBuilder
    UNKNOWN,                            // can't static_cast<>
    DONT_CARE                           // don't have to compare for equality
  };

  class CallParameters {
   public:
    CallParameters(const TimeRange& time_range,
                   int remove_mask,
                   int origin_type_mask,
                   std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                   FilterBuilderType type)
        : time_range_(time_range),
          remove_mask_(remove_mask),
          origin_type_mask_(origin_type_mask),
          filter_builder_(std::move(filter_builder)),
          type_(type) {}
    ~CallParameters() {}

    bool operator==(const CallParameters& other) const {
      const CallParameters& a = *this;
      const CallParameters& b = other;

      if (!(a.time_range_ == b.time_range_) ||
          a.remove_mask_ != b.remove_mask_ ||
          a.origin_type_mask_ != b.origin_type_mask_) {
        return false;
      }

      if (a.type_ == DONT_CARE || b.type_ == DONT_CARE)
        return true;
      if (a.type_ == UNKNOWN && b.type_ == UNKNOWN)
        return false;
      if (a.type_ != UNKNOWN && b.type_ != UNKNOWN && a.type_ != b.type_)
        return false;

      FilterBuilderType resolved_type =
          (a.type_ != UNKNOWN) ? a.type_ : b.type_;

      DCHECK(resolved_type == ORIGIN_FILTER_BUILDER ||
             resolved_type == REGISTRABLE_DOMAIN_FILTER_BUILDER);

      if (resolved_type == ORIGIN_FILTER_BUILDER) {
        return *static_cast<OriginFilterBuilder*>(a.filter_builder_.get()) ==
               *static_cast<OriginFilterBuilder*>(b.filter_builder_.get());
      } else if (resolved_type == REGISTRABLE_DOMAIN_FILTER_BUILDER) {
        return *static_cast<RegistrableDomainFilterBuilder*>(
                   a.filter_builder_.get()) ==
               *static_cast<RegistrableDomainFilterBuilder*>(
                   b.filter_builder_.get());
      }

      NOTREACHED();
      return false;
    }

   private:
    TimeRange time_range_;
    int remove_mask_;
    int origin_type_mask_;
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_;
    FilterBuilderType type_;
  };

  std::list<CallParameters> actual_calls_;
  std::list<CallParameters> expected_calls_;
};

// Tests for ChromeContentBrowserClient::ClearSiteData().
class ChromeContentBrowserClientClearSiteDataTest : public testing::Test {
 public:
  void SetUp() override {
    BrowsingDataRemoverFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, &ChromeContentBrowserClientClearSiteDataTest::GetRemover);
  }

  content::BrowserContext* profile() { return &profile_; }

  MockBrowsingDataRemover* remover() {
    return static_cast<MockBrowsingDataRemover*>(
        BrowsingDataRemoverFactory::GetForBrowserContext(&profile_));
  }

  void SetClearingFinished(bool finished) { finished_ = finished; }

  bool IsClearingFinished() { return finished_; }

 private:
  static std::unique_ptr<KeyedService> GetRemover(
      content::BrowserContext* context) {
    return base::WrapUnique(new MockBrowsingDataRemover(context));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  bool finished_;
};

// Tests that the parameters to ClearBrowsingData() are translated to
// the correct BrowsingDataRemover::RemoveInternal() operation. The fourth
// parameter, |filter_builder|, is tested in detail in the RegistrableDomains
// test below.
TEST_F(ChromeContentBrowserClientClearSiteDataTest, Parameters) {
  ChromeContentBrowserClient client;

  struct TestCase {
    bool cookies;
    bool storage;
    bool cache;
    int mask;
  } test_cases[] = {
      {false, false, false, 0},
      {true, false, false, BrowsingDataRemover::REMOVE_COOKIES |
                               BrowsingDataRemover::REMOVE_CHANNEL_IDS |
                               BrowsingDataRemover::REMOVE_PLUGIN_DATA},
      {false, true, false, BrowsingDataRemover::REMOVE_SITE_DATA &
                               ~BrowsingDataRemover::REMOVE_COOKIES &
                               ~BrowsingDataRemover::REMOVE_CHANNEL_IDS &
                               ~BrowsingDataRemover::REMOVE_PLUGIN_DATA},
      {false, false, true, BrowsingDataRemover::REMOVE_CACHE},
      {true, true, false, BrowsingDataRemover::REMOVE_SITE_DATA},
      {true, false, true, BrowsingDataRemover::REMOVE_COOKIES |
                              BrowsingDataRemover::REMOVE_CHANNEL_IDS |
                              BrowsingDataRemover::REMOVE_PLUGIN_DATA |
                              BrowsingDataRemover::REMOVE_CACHE},
      {false, true, true, BrowsingDataRemover::REMOVE_CACHE |
                              (BrowsingDataRemover::REMOVE_SITE_DATA &
                               ~BrowsingDataRemover::REMOVE_COOKIES &
                               ~BrowsingDataRemover::REMOVE_CHANNEL_IDS &
                               ~BrowsingDataRemover::REMOVE_PLUGIN_DATA)},
      {true, true, true, BrowsingDataRemover::REMOVE_SITE_DATA |
                             BrowsingDataRemover::REMOVE_CACHE},
  };

  for (unsigned int i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test case %d", i));
    const TestCase& test_case = test_cases[i];

    // We always delete data for all time and all origin types.
    BrowsingDataRemover::TimeRange all_time(base::Time(), base::Time::Max());
    BrowsingDataHelper::OriginTypeMask all_origin_types =
        BrowsingDataHelper::ALL;

    // Some data are deleted for the origin and some for the registrable domain.
    // Depending on the chosen datatypes, this might result into one or two
    // calls. In the latter case, the removal mask will be split into two
    // parts - one for the origin deletion and one for the registrable domain.
    const int domain_scoped_types = BrowsingDataRemover::REMOVE_COOKIES |
                                    BrowsingDataRemover::REMOVE_CHANNEL_IDS |
                                    BrowsingDataRemover::REMOVE_PLUGIN_DATA;
    int registrable_domain_deletion_mask = test_case.mask & domain_scoped_types;
    int origin_deletion_mask = test_case.mask & ~domain_scoped_types;

    if (registrable_domain_deletion_mask) {
      remover()->ExpectCallDontCareAboutFilterBuilder(
          all_time, registrable_domain_deletion_mask, all_origin_types);
    }

    if (origin_deletion_mask) {
      remover()->ExpectCallDontCareAboutFilterBuilder(
          all_time, origin_deletion_mask, all_origin_types);
    }

    SetClearingFinished(false);
    client.ClearSiteData(
        profile(), url::Origin(GURL("https://www.example.com")),
        test_case.cookies, test_case.storage, test_case.cache,
        base::Bind(
            &ChromeContentBrowserClientClearSiteDataTest::SetClearingFinished,
            base::Unretained(this), true));
    EXPECT_TRUE(IsClearingFinished());

    remover()->VerifyAndClearExpectations();
  }
}

// Tests that ClearBrowsingData() called for an origin deletes cookies in the
// scope of the registrable domain corresponding to that origin, while cache
// is deleted for that exact origin.
TEST_F(ChromeContentBrowserClientClearSiteDataTest, RegistrableDomains) {
  ChromeContentBrowserClient client;

  struct TestCase {
    const char* origin;  // origin on which ClearSiteData() is called.
    const char* domain;  // domain on which cookies will be deleted.
  } test_cases[] = {
      // TLD has no embedded dot.
      {"https://example.com", "example.com"},
      {"https://www.example.com", "example.com"},
      {"https://www.fourth.third.second.com", "second.com"},

      // TLD has one embedded dot.
      {"https://www.example.co.uk", "example.co.uk"},
      {"https://example.co.uk", "example.co.uk"},

      // TLD has more embedded dots.
      {"https://www.website.sp.nom.br", "website.sp.nom.br"},

      // IP addresses.
      {"http://127.0.0.1", "127.0.0.1"},
      {"http://192.168.0.1", "192.168.0.1"},
      {"http://192.168.0.1", "192.168.0.1"},

      // Internal hostnames.
      {"http://localhost", "localhost"},
      {"http://fileserver", "fileserver"},

      // These are not subdomains of internal hostnames, but subdomain of
      // unknown TLDs.
      {"http://subdomain.localhost", "subdomain.localhost"},
      {"http://www.subdomain.localhost", "subdomain.localhost"},
      {"http://documents.fileserver", "documents.fileserver"},

      // Scheme and port don't matter.
      {"http://example.com", "example.com"},
      {"http://example.com:8080", "example.com"},
      {"https://example.com:4433", "example.com"},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.origin);

    std::unique_ptr<RegistrableDomainFilterBuilder>
        registrable_domain_filter_builder(new RegistrableDomainFilterBuilder(
            BrowsingDataFilterBuilder::WHITELIST));
    registrable_domain_filter_builder->AddRegisterableDomain(test_case.domain);

    remover()->ExpectCall(
        BrowsingDataRemover::Period(browsing_data::TimePeriod::ALL_TIME),
        BrowsingDataRemover::REMOVE_COOKIES |
            BrowsingDataRemover::REMOVE_CHANNEL_IDS |
            BrowsingDataRemover::REMOVE_PLUGIN_DATA,
        BrowsingDataHelper::ALL, std::move(registrable_domain_filter_builder));

    std::unique_ptr<OriginFilterBuilder> origin_filter_builder(
        new OriginFilterBuilder(BrowsingDataFilterBuilder::WHITELIST));
    origin_filter_builder->AddOrigin(url::Origin(GURL(test_case.origin)));

    remover()->ExpectCall(
        BrowsingDataRemover::Period(browsing_data::TimePeriod::ALL_TIME),
        BrowsingDataRemover::REMOVE_CACHE, BrowsingDataHelper::ALL,
        std::move(origin_filter_builder));

    SetClearingFinished(false);
    client.ClearSiteData(
        profile(), url::Origin(GURL(test_case.origin)), true /* cookies */,
        false /* storage */, true /* cache */,
        base::Bind(
            &ChromeContentBrowserClientClearSiteDataTest::SetClearingFinished,
            base::Unretained(this), true));
    EXPECT_TRUE(IsClearingFinished());

    remover()->VerifyAndClearExpectations();
  }
}

// Tests that we always wait for all scheduled BrowsingDataRemover tasks and
// that BrowsingDataRemoverObserver never leaks.
TEST_F(ChromeContentBrowserClientClearSiteDataTest, Tasks) {
  ChromeContentBrowserClient client;
  url::Origin origin(GURL("https://www.example.com"));

  // No removal tasks.
  SetClearingFinished(false);
  client.ClearSiteData(
      profile(), origin, false /* cookies */, false /* storage */,
      false /* cache */,
      base::Bind(
          &ChromeContentBrowserClientClearSiteDataTest::SetClearingFinished,
          base::Unretained(this), true));
  EXPECT_TRUE(IsClearingFinished());

  // One removal task: deleting cookies with a domain filter.
  SetClearingFinished(false);
  client.ClearSiteData(
      profile(), origin, true /* cookies */, false /* storage */,
      false /* cache */,
      base::Bind(
          &ChromeContentBrowserClientClearSiteDataTest::SetClearingFinished,
          base::Unretained(this), true));
  EXPECT_TRUE(IsClearingFinished());

  // One removal task: deleting cache with a domain filter.
  SetClearingFinished(false);
  client.ClearSiteData(
      profile(), origin, false /* cookies */, false /* storage */,
      true /* cache */,
      base::Bind(
          &ChromeContentBrowserClientClearSiteDataTest::SetClearingFinished,
          base::Unretained(this), true));
  EXPECT_TRUE(IsClearingFinished());

  // Two removal tasks, with domain and origin filters respectively.
  SetClearingFinished(false);
  client.ClearSiteData(
      profile(), origin, true /* cookies */, false /* storage */,
      true /* cache */,
      base::Bind(
          &ChromeContentBrowserClientClearSiteDataTest::SetClearingFinished,
          base::Unretained(this), true));
  EXPECT_TRUE(IsClearingFinished());
}

}  // namespace
