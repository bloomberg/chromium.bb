// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/webshare_target.h"
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
constexpr char kUrlTemplateHigh[] =
    "https://www.example-high.com/target/"
    "share?title={title}&text={text}&url={url}";
constexpr char kUrlTemplateLow[] =
    "https://www.example-low.com/target/"
    "share?title={title}&text={text}&url={url}";
constexpr char kUrlTemplateMin[] =
    "https://www.example-min.com/target/"
    "share?title={title}&text={text}&url={url}";
constexpr char kManifestUrlHigh[] =
    "https://www.example-high.com/target/manifest.json";
constexpr char kManifestUrlLow[] =
    "https://www.example-low.com/target/manifest.json";
constexpr char kManifestUrlMin[] =
    "https://www.example-min.com/target/manifest.json";

void DidShare(blink::mojom::ShareError expected_error,
              blink::mojom::ShareError error) {
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

    origin_dict->SetKey(kUrlTemplateKey, base::Value(url_template));
    origin_dict->SetKey(kNameKey, base::Value(name));

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

  const std::vector<WebShareTarget>& GetTargetsInPicker() {
    return targets_in_picker_;
  }

  void PickTarget(const std::string& target_url) {
    const auto& it =
        std::find_if(targets_in_picker_.begin(), targets_in_picker_.end(),
                     [&target_url](const WebShareTarget& target) {
                       return target.manifest_url().spec() == target_url;
                     });
    DCHECK(it != targets_in_picker_.end());
    std::move(picker_callback_).Run(&*it);
  }

  chrome::WebShareTargetPickerCallback picker_callback() {
    return std::move(picker_callback_);
  }

 private:
  void ShowPickerDialog(
      std::vector<WebShareTarget> targets,
      chrome::WebShareTargetPickerCallback callback) override {
    // Store the arguments passed to the picker dialog.
    targets_in_picker_ = std::move(targets);
    picker_callback_ = std::move(callback);

    // Quit the test's run loop. It is the test's responsibility to call the
    // callback, to simulate the user's choice.
    std::move(quit_run_loop_).Run();
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
  base::OnceClosure quit_run_loop_;

  // The last URL passed to OpenTargetURL.
  std::string last_used_target_url_;
  // The targets passed to ShowPickerDialog.
  std::vector<WebShareTarget> targets_in_picker_;
  // The callback passed to ShowPickerDialog (which is supposed to be called
  // with the user's chosen result, or nullptr if cancelled).
  chrome::WebShareTargetPickerCallback picker_callback_;
};

class ShareServiceImplUnittest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceImplUnittest() = default;
  ~ShareServiceImplUnittest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    share_service_helper_ = std::make_unique<ShareServiceTestImpl>(
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
                                                kUrlTemplateLow);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kUrlTemplateHigh);
  // Expect this invalid URL to be ignored (not crash);
  // https://crbug.com/762388.
  share_service_helper()->AddShareTargetToPrefs("", kTargetName,
                                                kUrlTemplateHigh);

  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::OK);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlHigh), kTargetName,
                                GURL(kUrlTemplateHigh));
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kUrlTemplateLow));
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->PickTarget(kManifestUrlLow);

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
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::CANCELED);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  EXPECT_TRUE(share_service_helper()->GetTargetsInPicker().empty());

  // Cancel the dialog.
  share_service_helper()->picker_callback().Run(nullptr);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Tests the result of cancelling the share in the picker UI, that has targets.
TEST_F(ShareServiceImplUnittest, ShareCancelWithTargets) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kUrlTemplateHigh);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kUrlTemplateLow);

  // Expect an error message in response.
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::CANCELED);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlHigh), kTargetName,
                                GURL(kUrlTemplateHigh));
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kUrlTemplateLow));
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Cancel the dialog.
  share_service_helper()->picker_callback().Run(nullptr);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Tests a target with a broken URL template (ReplacePlaceholders failure).
TEST_F(ShareServiceImplUnittest, ShareBrokenUrl) {
  // Invalid placeholders. Detailed tests for broken templates are in the
  // ReplacePlaceholders test; this just tests the share response.
  constexpr char kBrokenUrlTemplate[] =
      "http://webshare.com/share?title={title";
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kBrokenUrlTemplate);

  // Expect an error message in response.
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::INTERNAL_ERROR);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlHigh), kTargetName,
                                GURL(kBrokenUrlTemplate));
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Pick example-high.com.
  share_service_helper()->PickTarget(kManifestUrlHigh);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Test to check that only targets with enough engagement were in picker.
TEST_F(ShareServiceImplUnittest, ShareWithSomeInsufficientlyEngagedTargets) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlMin, kTargetName,
                                                kUrlTemplateMin);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kUrlTemplateLow);

  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::OK);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kUrlTemplateLow));
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->PickTarget(kManifestUrlLow);

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
                                                kUrlTemplateLow);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  // Expect the callback to never be called (since the share service is
  // destroyed before the picker is closed).
  // TODO(mgiuca): This probably should still complete the share, if not
  // cancelled, even if the underlying tab is closed.
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce([](blink::mojom::ShareError error) { FAIL(); });
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kUrlTemplateLow));
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  chrome::WebShareTargetPickerCallback picker_callback =
      share_service_helper()->picker_callback();

  DeleteShareService();

  // Pick example-low.com.
  std::move(picker_callback).Run(&expected_targets[0]);
}

TEST_F(ShareServiceImplUnittest, ReplaceUrlPlaceholdersInvalidTemplate) {
  const GURL kUrl(kUrlSpec);
  GURL url_template_filled;

  // Badly nested placeholders.
  GURL url_template = GURL("http://example.com/?q={");
  bool succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = GURL("http://example.com/?q={title");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = GURL("http://example.com/?q={title{text}}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = GURL("http://example.com/?q={title{}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);

  url_template = GURL("http://example.com/?q={{title}}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);

  // Placeholder with non-identifier character.
  url_template = GURL("http://example.com/?q={title?}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);

  // Invalid placeholder in URL fragment.
  url_template = GURL("http://example.com/#{title?}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_FALSE(succeeded);
}

TEST_F(ShareServiceImplUnittest, ReplaceUrlPlaceholders) {
  const GURL kUrl(kUrlSpec);

  // No placeholders.
  GURL url_template = GURL("http://example.com/?q=a#a");
  GURL url_template_filled;
  bool succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(url_template, url_template_filled);

  // Empty |url_template|
  url_template = GURL();
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(GURL(), url_template_filled);

  // One title placeholder.
  url_template = GURL("http://example.com/#{title}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#My%20title", url_template_filled.spec());

  // One text placeholder.
  url_template = GURL("http://example.com/#{text}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#My%20text", url_template_filled.spec());

  // One url placeholder.
  url_template = GURL("http://example.com/#{url}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#https%3A%2F%2Fwww.google.com%2F",
            url_template_filled.spec());

  // One of each placeholder, in title, text, url order.
  url_template = GURL("http://example.com/#{title}{text}{url}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "http://example.com/#My%20titleMy%20texthttps%3A%2F%2Fwww.google.com%2F",
      url_template_filled.spec());

  // One of each placeholder, in url, text, title order.
  url_template = GURL("http://example.com/#{url}{text}{title}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "http://example.com/#https%3A%2F%2Fwww.google.com%2FMy%20textMy%20title",
      url_template_filled.spec());

  // Two of each placeholder, some next to each other, others not.
  url_template =
      GURL("http://example.com/#{title}{url}{text}{text}{title}{url}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "http://example.com/"
      "#My%20titlehttps%3A%2F%2Fwww.google.com%2FMy%20textMy%20textMy%"
      "20titlehttps%3A%2F%2Fwww.google.com%2F",
      url_template_filled.spec());

  // Placeholders are in a query string, as values. The expected use case.
  // Two of each placeholder, some next to each other, others not.
  url_template = GURL(
      "http://example.com?title={title}&url={url}&text={text}&text={text}&"
      "title={title}&url={url}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "http://"
      "example.com/?title=My%20title&url=https%3A%2F%2Fwww.google.com%2F&"
      "text=My%20text&"
      "text=My%20text&title=My%20title&url=https%3A%2F%2Fwww.google.com%2F",
      url_template_filled.spec());

  // Placeholder with digit character.
  url_template = GURL("http://example.com/#{title1}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#", url_template_filled.spec());

  // Empty placeholder.
  url_template = GURL("http://example.com/#{}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#", url_template_filled.spec());

  // Unexpected placeholders.
  url_template = GURL("http://example.com/#{nonexistentplaceholder}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#", url_template_filled.spec());

  // Placeholders should only be replaced in query and fragment.
  url_template = GURL("http://example.com/subpath{title}/?q={title}#{title}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/subpath%7Btitle%7D/?q=My%20title#My%20title",
            url_template_filled.spec());

  // Braces in the path, which would be invalid, but should parse fine as they
  // are escaped.
  url_template = GURL("http://example.com/subpath{/?q={title}");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/subpath%7B/?q=My%20title",
            url_template_filled.spec());

  // |url_template| with % escapes.
  url_template = GURL("http://example.com#%20{title}%20");
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      url_template, kTitle, kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#%20My%20title%20", url_template_filled.spec());
}

// Test URL escaping done by ReplaceUrlPlaceholders().
TEST_F(ShareServiceImplUnittest, ReplaceUrlPlaceholders_Escaping) {
  const GURL kUrl(kUrlSpec);
  const GURL kUrlTemplate("http://example.com/#{title}");

  // Share data that contains percent escapes.
  GURL url_template_filled;
  bool succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate, "My%20title", kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#My%2520title", url_template_filled.spec());

  // Share data that contains placeholders. These should not be replaced.
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate, "{title}", kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#%7Btitle%7D", url_template_filled.spec());

  // All characters that shouldn't be escaped.
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate,
      "-_.!~*'()0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz",
      kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "http://example.com/#-_.!~*'()0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz",
      url_template_filled.spec());

  // All characters that should be escaped.
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate, " \"#$%&+,/:;<=>?@[\\]^`{|}", kText, kUrl,
      &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(
      "http://example.com/"
      "#%20%22%23%24%25%26%2B%2C%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%60%7B%7C%"
      "7D",
      url_template_filled.spec());

  // Unicode chars.
  // U+263B
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate, "\xe2\x98\xbb", kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#%E2%98%BB", url_template_filled.spec());

  // U+00E9
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate, "\xc3\xa9", kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#%C3%A9", url_template_filled.spec());

  // U+1F4A9
  succeeded = ShareServiceImpl::ReplaceUrlPlaceholders(
      kUrlTemplate, "\xf0\x9f\x92\xa9", kText, kUrl, &url_template_filled);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ("http://example.com/#%F0%9F%92%A9", url_template_filled.spec());
}
