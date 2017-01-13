// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTitle[] = "My title";
constexpr char kText[] = "My text";
constexpr char kUrlSpec[] = "https://www.google.com/";

class ShareServiceTestImpl : public ShareServiceImpl {
 public:
  static ShareServiceTestImpl* Create(
      blink::mojom::ShareServiceRequest request) {
    std::unique_ptr<ShareServiceTestImpl> share_service_helper =
        base::MakeUnique<ShareServiceTestImpl>();
    ShareServiceTestImpl* share_service_helper_raw = share_service_helper.get();
    mojo::MakeStrongBinding(std::move(share_service_helper),
                            std::move(request));
    return share_service_helper_raw;
  }

  const std::string& GetLastUsedTargetURL() { return last_used_target_url_; }

 private:
  std::string last_used_target_url_;

  void OpenTargetURL(const GURL& target_url) override {
    last_used_target_url_ = target_url.spec();
  }
};

class ShareServiceImplUnittest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceImplUnittest() = default;
  ~ShareServiceImplUnittest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    share_service_helper_ =
        ShareServiceTestImpl::Create(mojo::MakeRequest(&share_service_));
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  void DidShare(const std::string& expected_target_url,
                const base::Optional<std::string>& expected_param,
                const base::Optional<std::string>& param) {
    EXPECT_EQ(expected_param, param);
    std::string target_url = share_service_helper_->GetLastUsedTargetURL();
    EXPECT_EQ(expected_target_url, target_url);

    if (!on_callback_.is_null())
      on_callback_.Run();
  }

  blink::mojom::ShareServicePtr share_service_;
  ShareServiceTestImpl* share_service_helper_;
  base::Closure on_callback_;
};

}  // namespace

// Basic test to check the Share method calls the callback with the expected
// parameters.
TEST_F(ShareServiceImplUnittest, ShareCallbackParams) {
  std::string expected_url =
      "https://wicg.github.io/web-share-target/demos/"
      "sharetarget.html?title=My%20title&text=My%20text&url=https%3A%2F%2Fwww."
      "google.com%2F";

  const GURL url(kUrlSpec);
  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(&ShareServiceImplUnittest::DidShare, base::Unretained(this),
                 expected_url, base::Optional<std::string>());

  base::RunLoop run_loop;
  on_callback_ = run_loop.QuitClosure();

  share_service_->Share(kTitle, kText, url, callback);

  run_loop.Run();
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
