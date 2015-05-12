// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/extension_badged_icon_image.h"

#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

using chromeos::launcher_search_provider::ErrorReporter;

namespace app_list {

namespace {

const char kTestExtensionId[] = "foo";
const char kTestCustomIconURL[] = "chrome-extension://foo/bar";

// This image source generates following image:
//
// ###
// #**
// #**
//
// where # is the primary fill color, and * is the secondary fill color.
class BadgedImageSource : public gfx::CanvasImageSource {
 public:
  BadgedImageSource(const gfx::Size& image_size,
                    const SkColor primary_fill_color)
      : CanvasImageSource(image_size, false),
        primary_fill_color_(primary_fill_color),
        secondary_fill_color_(SK_ColorTRANSPARENT) {}

  BadgedImageSource(const gfx::Size& image_size,
                    const SkColor primary_fill_color,
                    const SkColor secondary_fill_color)
      : CanvasImageSource(image_size, false),
        primary_fill_color_(primary_fill_color),
        secondary_fill_color_(secondary_fill_color) {}

  void Draw(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(size()), primary_fill_color_);

    const int badge_dimension = size().width() * 2 / 3;
    const int offset = size().width() - badge_dimension;
    canvas->FillRect(
        gfx::Rect(offset, offset, badge_dimension, badge_dimension),
        secondary_fill_color_);
  }

 private:
  const SkColor primary_fill_color_;
  const SkColor secondary_fill_color_;

  DISALLOW_COPY_AND_ASSIGN(BadgedImageSource);
};

// Test implementation of ExtensionBadgedIconImage.
class ExtensionBadgedIconImageTestImpl : public ExtensionBadgedIconImage {
 public:
  // Use base class constructor.
  using ExtensionBadgedIconImage::ExtensionBadgedIconImage;

  const gfx::ImageSkia& LoadExtensionIcon() override {
    // Returns 32x32 black image.
    extension_icon_ = gfx::ImageSkia(
        new BadgedImageSource(icon_size_, SK_ColorBLACK), icon_size_);
    return extension_icon_;
  }

  // Calls OnExtensionIconImageChnaged callback with |extension_icon|.
  void LoadExtensionIconAsync(const gfx::ImageSkia& image) {
    OnExtensionIconChanged(image);
  }

  void LoadIconResourceFromExtension() override {
    // For success case, returns 32x32 blue image.
    is_load_extension_icon_resource_called_ = true;
  }

  bool IsLoadExtensionIconResourceCalled() const {
    return is_load_extension_icon_resource_called_;
  }

  // Calls OnCustomIconLoaded callback with |custom_icon|. Sets an empty image
  // for simulating a failure case.
  void CallOnCustomIconLoaded(gfx::ImageSkia custom_icon) {
    OnCustomIconLoaded(custom_icon);
  }

 private:
  gfx::ImageSkia extension_icon_;
  bool is_load_extension_icon_resource_called_ = false;
};

// A fake error reporter to test error message.
class FakeErrorReporter : public ErrorReporter {
 public:
  FakeErrorReporter() : ErrorReporter(nullptr, -1) {
    last_message_.reset(new std::string());
  }
  explicit FakeErrorReporter(const linked_ptr<std::string>& last_message)
      : ErrorReporter(nullptr, -1), last_message_(last_message) {}
  ~FakeErrorReporter() override {}
  void Warn(const std::string& message) override {
    last_message_->clear();
    last_message_->append(message);
  }

  const std::string& GetLastWarningMessage() { return *last_message_.get(); }

  scoped_ptr<ErrorReporter> Duplicate() override {
    return make_scoped_ptr(new FakeErrorReporter(last_message_));
  }

 private:
  linked_ptr<std::string> last_message_;

  DISALLOW_COPY_AND_ASSIGN(FakeErrorReporter);
};

// Creates a test extension with |extension_id|.
scoped_refptr<extensions::Extension> CreateTestExtension(
    const std::string& extension_id) {
  base::DictionaryValue manifest;
  std::string error;
  manifest.SetStringWithoutPathExpansion(extensions::manifest_keys::kVersion,
                                         "1");
  manifest.SetStringWithoutPathExpansion(extensions::manifest_keys::kName,
                                         "TestExtension");
  return extensions::Extension::Create(
      base::FilePath(), extensions::Manifest::UNPACKED, manifest,
      extensions::Extension::NO_FLAGS, extension_id, &error);
}

// Returns true if icon image of |badged_icon_image| equals to |expected_image|.
bool IsEqual(const gfx::ImageSkia& expected_image,
             const ExtensionBadgedIconImage& badged_icon_image) {
  return gfx::test::IsEqual(
      expected_image.GetRepresentation(1.0).sk_bitmap(),
      badged_icon_image.GetIconImage().GetRepresentation(1.0).sk_bitmap());
}

}  // namespace

class ExtensionBadgedIconImageTest : public testing::Test {
 protected:
  void SetUp() override { extension_ = CreateTestExtension(kTestExtensionId); }

  scoped_ptr<FakeErrorReporter> GetFakeErrorReporter() {
    return make_scoped_ptr(new FakeErrorReporter());
  }

  scoped_refptr<extensions::Extension> extension_;
};

TEST_F(ExtensionBadgedIconImageTest, WithoutCustomIconSuccessCase) {
  GURL icon_url;  // No custom icon.
  ExtensionBadgedIconImageTestImpl impl(icon_url, nullptr, nullptr, 32,
                                        GetFakeErrorReporter());
  impl.LoadResources();

  // Icon should be black image.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia expected_image(new BadgedImageSource(icon_size, SK_ColorBLACK),
                                icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl));
}

TEST_F(ExtensionBadgedIconImageTest, ExtensionIconAsyncLoadSuccessCase) {
  GURL icon_url;  // No custom icon.
  ExtensionBadgedIconImageTestImpl impl(icon_url, nullptr, nullptr, 32,
                                        GetFakeErrorReporter());
  impl.LoadResources();

  // Extension icon is loaded as async.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia extension_icon(new BadgedImageSource(icon_size, SK_ColorGREEN),
                                icon_size);
  impl.LoadExtensionIconAsync(extension_icon);

  gfx::ImageSkia expected_image(new BadgedImageSource(icon_size, SK_ColorGREEN),
                                icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl));
}

TEST_F(ExtensionBadgedIconImageTest, WithCustomIconSuccessCase) {
  GURL icon_url(kTestCustomIconURL);
  ExtensionBadgedIconImageTestImpl impl(icon_url, nullptr, extension_.get(), 32,
                                        GetFakeErrorReporter());
  ASSERT_FALSE(impl.IsLoadExtensionIconResourceCalled());
  impl.LoadResources();

  // Asserts that LoadExtensionIconResource is called.
  ASSERT_TRUE(impl.IsLoadExtensionIconResourceCalled());

  // Load custom icon as async.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia custom_icon(new BadgedImageSource(icon_size, SK_ColorGREEN),
                             icon_size);
  impl.CallOnCustomIconLoaded(custom_icon);

  gfx::ImageSkia expected_image(
      new BadgedImageSource(icon_size, SK_ColorGREEN, SK_ColorBLACK),
      icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl));
}

TEST_F(ExtensionBadgedIconImageTest, InvalidCustomIconUrl) {
  // Use a really long URL (for testing the string truncation).
  // The URL is from the wrong extension (foo2), so should be rejected.
  std::string invalid_url =
      "chrome-extension://foo2/bar/"
      "901234567890123456789012345678901234567890123456789012345678901234567890"
      "1";
  ASSERT_EQ(101U, invalid_url.size());

  scoped_ptr<FakeErrorReporter> fake_error_reporter = GetFakeErrorReporter();
  GURL icon_url(invalid_url);
  ExtensionBadgedIconImageTestImpl impl(icon_url, nullptr, extension_.get(), 32,
                                        fake_error_reporter->Duplicate());
  impl.LoadResources();

  // Warning message should be provided.
  ASSERT_EQ(
      "[chrome.launcherSearchProvider.setSearchResults] Invalid icon URL: "
      "chrome-extension://foo2/bar/"
      "901234567890123456789012345678901234567890123456789012345678901234567..."
      ". Must have a valid URL within chrome-extension://foo.",
      fake_error_reporter->GetLastWarningMessage());

  // LoadExtensionIconResource should not be called.
  ASSERT_FALSE(impl.IsLoadExtensionIconResourceCalled());
}

TEST_F(ExtensionBadgedIconImageTest, FailedToLoadCustomIcon) {
  scoped_ptr<FakeErrorReporter> fake_error_reporter = GetFakeErrorReporter();
  GURL icon_url(kTestCustomIconURL);
  ExtensionBadgedIconImageTestImpl impl(icon_url, nullptr, extension_.get(), 32,
                                        fake_error_reporter->Duplicate());
  impl.LoadResources();
  ASSERT_TRUE(impl.IsLoadExtensionIconResourceCalled());

  // Fails to load custom icon by passing an empty image.
  gfx::ImageSkia custom_icon;
  impl.CallOnCustomIconLoaded(custom_icon);

  // Warning message should be shown.
  ASSERT_EQ(
      "[chrome.launcherSearchProvider.setSearchResults] Failed to load icon "
      "URL: chrome-extension://foo/bar",
      fake_error_reporter->GetLastWarningMessage());

  // Icon should be extension icon.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia expected_image(new BadgedImageSource(icon_size, SK_ColorBLACK),
                                icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl));
}

}  // namespace app_list
