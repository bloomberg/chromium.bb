// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTitle[] = "My title";
constexpr char kText[] = "My text";
constexpr char kUrlSpec[] = "https://www.google.com/";

constexpr char kTargetName[] = "Share Target";
constexpr char kUrlTemplate[] = "share?title={title}&text={text}&url={url}";
constexpr char kManifestUrlHigh[] =
    "https://www.example-high.com/target/manifest.json";
constexpr char kManifestUrlLow[] =
    "https://www.example-low.com/target/manifest.json";
constexpr char kManifestUrlMin[] =
    "https://www.example-min.com/target/manifest.json";

void DidShare(const base::Optional<std::string>& expected_error,
              const base::Optional<std::string>& error) {
  EXPECT_EQ(expected_error, error);
}

class ShareServiceTestImpl : public ShareServiceImpl {
 public:
  explicit ShareServiceTestImpl(blink::mojom::ShareServiceRequest request)
      : binding_(this) {
    binding_.Bind(std::move(request));

    pref_service_.reset(new TestingPrefServiceSimple());
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kWebShareVisitedTargets);
  }

  void AddShareTargetToPrefs(const std::string& manifest_url,
                             const std::string& name,
                             const std::string& url_template) {
    constexpr char kUrlTemplateKey[] = "url_template";
    constexpr char kNameKey[] = "name";

    DictionaryPrefUpdate update(GetPrefService(),
                                prefs::kWebShareVisitedTargets);
    base::DictionaryValue* share_target_dict = update.Get();

    std::unique_ptr<base::DictionaryValue> origin_dict(
        new base::DictionaryValue);

    origin_dict->SetStringWithoutPathExpansion(kUrlTemplateKey, url_template);
    origin_dict->SetStringWithoutPathExpansion(kNameKey, name);

    share_target_dict->SetWithoutPathExpansion(manifest_url,
                                               std::move(origin_dict));
  }

  void SetEngagementForTarget(const std::string& manifest_url,
                              blink::mojom::EngagementLevel level) {
    engagement_map_[manifest_url] = level;
  }

  void set_run_loop(base::RunLoop* run_loop) {
    quit_run_loop_ = run_loop->QuitClosure();
  }

  const std::string& GetLastUsedTargetURL() { return last_used_target_url_; }

  const std::vector<std::pair<base::string16, GURL>>& GetTargetsInPicker() {
    return targets_in_picker_;
  }

  const base::Callback<void(base::Optional<std::string>)>& picker_callback() {
    return picker_callback_;
  }

 private:
  void ShowPickerDialog(
      const std::vector<std::pair<base::string16, GURL>>& targets,
      const base::Callback<void(base::Optional<std::string>)>& callback)
      override {
    // Store the arguments passed to the picker dialog.
    targets_in_picker_ = targets;
    picker_callback_ = callback;

    // Quit the test's run loop. It is the test's responsibility to call the
    // callback, to simulate the user's choice.
    quit_run_loop_.Run();
  }

  void OpenTargetURL(const GURL& target_url) override {
    last_used_target_url_ = target_url.spec();
  }

  PrefService* GetPrefService() override { return pref_service_.get(); }

  blink::mojom::EngagementLevel GetEngagementLevel(const GURL& url) override {
    return engagement_map_[url.spec()];
  }

  mojo::Binding<blink::mojom::ShareService> binding_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::map<std::string, blink::mojom::EngagementLevel> engagement_map_;
  // Closure to quit the test's run loop.
  base::Closure quit_run_loop_;

  // The last URL passed to OpenTargetURL.
  std::string last_used_target_url_;
  // The targets passed to ShowPickerDialog.
  std::vector<std::pair<base::string16, GURL>> targets_in_picker_;
  // The callback passed to ShowPickerDialog (which is supposed to be called
  // with the user's chosen result, or nullopt if cancelled).
  base::Callback<void(base::Optional<std::string>)> picker_callback_;
};

class ShareServiceImplUnittest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceImplUnittest() = default;
  ~ShareServiceImplUnittest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    share_service_helper_ = base::MakeUnique<ShareServiceTestImpl>(
        mojo::MakeRequest(&share_service_));

    share_service_helper_->SetEngagementForTarget(
        kManifestUrlHigh, blink::mojom::EngagementLevel::HIGH);
    share_service_helper_->SetEngagementForTarget(
        kManifestUrlMin, blink::mojom::EngagementLevel::MINIMAL);
    share_service_helper_->SetEngagementForTarget(
        kManifestUrlLow, blink::mojom::EngagementLevel::LOW);
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  blink::mojom::ShareService* share_service() const {
    return share_service_.get();
  }

  ShareServiceTestImpl* share_service_helper() const {
    return share_service_helper_.get();
  }

  void DeleteShareService() { share_service_helper_.reset(); }

 private:
  blink::mojom::ShareServicePtr share_service_;
  std::unique_ptr<ShareServiceTestImpl> share_service_helper_;
};

}  // namespace

// Basic test to check the Share method calls the callback with the expected
// parameters.
TEST_F(ShareServiceImplUnittest, ShareCallbackParams) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kUrlTemplate);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kUrlTemplate);

  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(&DidShare, base::Optional<std::string>());

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, callback);

  run_loop.Run();

  const std::vector<std::pair<base::string16, GURL>> kExpectedTargets{
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlHigh)),
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlLow))};
  EXPECT_EQ(kExpectedTargets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->picker_callback().Run(
      base::Optional<std::string>(kManifestUrlLow));

  const char kExpectedURL[] =
      "https://www.example-low.com/target/"
      "share?title=My%20title&text=My%20text&url=https%3A%2F%2Fwww."
      "google.com%2F";
  EXPECT_EQ(kExpectedURL, share_service_helper()->GetLastUsedTargetURL());
}

// Tests the result of cancelling the share in the picker UI, that doesn't have
// any targets.
TEST_F(ShareServiceImplUnittest, ShareCancelNoTargets) {
  // Expect an error message in response.
  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(&DidShare, base::Optional<std::string>("Share was cancelled"));

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, callback);

  run_loop.Run();

  EXPECT_TRUE(share_service_helper()->GetTargetsInPicker().empty());

  // Cancel the dialog.
  share_service_helper()->picker_callback().Run(base::nullopt);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Tests the result of cancelling the share in the picker UI, that has targets.
TEST_F(ShareServiceImplUnittest, ShareCancelWithTargets) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kUrlTemplate);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kUrlTemplate);

  // Expect an error message in response.
  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(&DidShare, base::Optional<std::string>("Share was cancelled"));

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, callback);

  run_loop.Run();

  const std::vector<std::pair<base::string16, GURL>> kExpectedTargets{
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlHigh)),
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlLow))};
  EXPECT_EQ(kExpectedTargets, share_service_helper()->GetTargetsInPicker());

  // Cancel the dialog.
  share_service_helper()->picker_callback().Run(base::nullopt);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Tests a target with a broken URL template (ReplacePlaceholders failure).
TEST_F(ShareServiceImplUnittest, ShareBrokenUrl) {
  // Invalid placeholders. Detailed tests for broken templates are in the
  // ReplacePlaceholders test; this just tests the share response.
  constexpr char kBrokenUrlTemplate[] = "share?title={title";
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kBrokenUrlTemplate);

  // Expect an error message in response.
  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(&DidShare,
                 base::Optional<std::string>(
                     "Error: unable to replace placeholders in url template"));

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, callback);

  run_loop.Run();

  const std::vector<std::pair<base::string16, GURL>> kExpectedTargets{
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlHigh))};
  EXPECT_EQ(kExpectedTargets, share_service_helper()->GetTargetsInPicker());

  // Pick example-high.com.
  share_service_helper()->picker_callback().Run(
      base::Optional<std::string>(kManifestUrlHigh));

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Test to check that only targets with enough engagement were in picker.
TEST_F(ShareServiceImplUnittest, ShareWithSomeInsufficientlyEngagedTargets) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlMin, kTargetName,
                                                kUrlTemplate);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kUrlTemplate);

  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(&DidShare, base::Optional<std::string>());

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, callback);

  run_loop.Run();

  const std::vector<std::pair<base::string16, GURL>> kExpectedTargets{
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlLow))};
  EXPECT_EQ(kExpectedTargets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->picker_callback().Run(
      base::Optional<std::string>(kManifestUrlLow));

  const char kExpectedURL[] =
      "https://www.example-low.com/target/"
      "share?title=My%20title&text=My%20text&url=https%3A%2F%2Fwww."
      "google.com%2F";
  EXPECT_EQ(kExpectedURL, share_service_helper()->GetLastUsedTargetURL());
}

// Test that deleting the share service while the picker is open does not crash
// (https://crbug.com/690775).
TEST_F(ShareServiceImplUnittest, ShareServiceDeletion) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kUrlTemplate);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  // Expect the callback to never be called (since the share service is
  // destroyed before the picker is closed).
  // TODO(mgiuca): This probably should still complete the share, if not
  // cancelled, even if the underlying tab is closed.
  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind([](const base::Optional<std::string>& error) { FAIL(); });
  share_service()->Share(kTitle, kText, url, callback);

  run_loop.Run();

  const std::vector<std::pair<base::string16, GURL>> kExpectedTargets{
      make_pair(base::UTF8ToUTF16(kTargetName), GURL(kManifestUrlLow))};
  EXPECT_EQ(kExpectedTargets, share_service_helper()->GetTargetsInPicker());

  const base::Callback<void(base::Optional<std::string>)> picker_callback =
      share_service_helper()->picker_callback();

  DeleteShareService();

  // Pick example-low.com.
  picker_callback.Run(base::Optional<std::string>(kManifestUrlLow));
}

// Replace various numbers of placeholders in various orders. Placeholders are
// adjacent to eachother; there are no padding characters.
TEST_F(ShareServiceImplUnittest, ReplacePlaceholders) {
  const GURL url(kUrlSpec);
  std::string url_template_filled;
  bool succeeded;

  // No placeholders
  std::string url_template = "blank";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("blank", url_template_filled);

  // Empty |url_template|
  url_template = "";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("", url_template_filled);

  // One title placeholder.
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("My%20title", url_template_filled);

  // One text placeholder.
  url_template = "{text}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("My%20text", url_template_filled);

  // One url placeholder.
  url_template = "{url}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("https%3A%2F%2Fwww.google.com%2F", url_template_filled);

  // One of each placeholder, in title, text, url order.
  url_template = "{title}{text}{url}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("My%20titleMy%20texthttps%3A%2F%2Fwww.google.com%2F",
            url_template_filled);

  // One of each placeholder, in url, text, title order.
  url_template = "{url}{text}{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("https%3A%2F%2Fwww.google.com%2FMy%20textMy%20title",
            url_template_filled);

  // Two of each placeholder, some next to each other, others not.
  url_template = "{title}{url}{text}{text}{title}{url}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "My%20titlehttps%3A%2F%2Fwww.google.com%2FMy%20textMy%20textMy%20"
      "titlehttps%3A%2F%2Fwww.google.com%2F",
      url_template_filled);

  // Placeholders are in a query string, as values. The expected use case.
  // Two of each placeholder, some next to each other, others not.
  url_template =
      "?title={title}&url={url}&text={text}&text={text}&"
      "title={title}&url={url}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "?title=My%20title&url=https%3A%2F%2Fwww.google.com%2F&text=My%20text&"
      "text=My%20text&title=My%20title&url=https%3A%2F%2Fwww.google.com%2F",
      url_template_filled);

  // Badly nested placeholders.
  url_template = "{";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = "{title";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = "{title{text}}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = "{title{}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = "{title}}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_FALSE(succeeded);

  // Placeholder with non-identifier character.
  url_template = "{title?}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_FALSE(succeeded);

  // Placeholder with digit character.
  url_template = "{title1}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("", url_template_filled);

  // Empty placeholder.
  url_template = "{}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("", url_template_filled);

  // Unexpected placeholders.
  url_template = "{nonexistentplaceholder}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("", url_template_filled);

  // |url_template| with % escapes.
  url_template = "%20{title}%20";
  succeeded = ShareServiceImpl::ReplacePlaceholders(url_template, kTitle, kText,
                                                    url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("%20My%20title%20", url_template_filled);

  // Share data that contains percent escapes.
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(
      url_template, "My%20title", kText, url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("My%2520title", url_template_filled);

  // Share data that contains placeholders. These should not be replaced.
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(
      url_template, "{title}", kText, url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("%7Btitle%7D", url_template_filled);

  // All characters that shouldn't be escaped.
  url_template = "{title}";
  succeeded =
      ShareServiceImpl::ReplacePlaceholders(url_template,
                                            "-_.!~*'()0123456789"
                                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz",
                                            kText, url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "-_.!~*'()0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz",
      url_template_filled);

  // All characters that should be escaped.
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(
      url_template, " \"#$%&+,/:;<=>?@[\\]^`{|}", kText, url,
      &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "%20%22%23%24%25%26%2B%2C%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%60%7B%7C%"
      "7D",
      url_template_filled);

  // Unicode chars.
  // U+263B
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(
      url_template, "\xe2\x98\xbb", kText, url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("%E2%98%BB", url_template_filled);

  // U+00E9
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(
      url_template, "\xc3\xa9", kText, url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("%C3%A9", url_template_filled);

  // U+1F4A9
  url_template = "{title}";
  succeeded = ShareServiceImpl::ReplacePlaceholders(
      url_template, "\xf0\x9f\x92\xa9", kText, url, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("%F0%9F%92%A9", url_template_filled);
}
