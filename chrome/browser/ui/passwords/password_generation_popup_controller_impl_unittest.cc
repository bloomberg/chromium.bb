// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

class PasswordGenerationPopupControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<password_manager::PasswordManagerDriver> CreateDriver();
};

std::unique_ptr<password_manager::PasswordManagerDriver>
PasswordGenerationPopupControllerImplTest::CreateDriver() {
  return std::make_unique<password_manager::StubPasswordManagerDriver>();
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateTheSame) {
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), 20 /*max_length*/, base::ASCIIToUTF16("element"),
      100 /*generation_element_id*/, base::i18n::TextDirection(),
      autofill::PasswordForm());
  ui_data.password_form.username_value = base::ASCIIToUTF16("Name");
  ui_data.password_form.password_value = base::ASCIIToUTF16("12345");
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_EQ(controller1.get(), controller2.get());
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentBounds) {
  gfx::RectF rect(100, 20);
  autofill::password_generation::PasswordGenerationUIData ui_data(
      rect, 20 /*max_length*/, base::ASCIIToUTF16("element"),
      100 /*generation_element_id*/, base::i18n::TextDirection(),
      autofill::PasswordForm());
  ui_data.password_form.username_value = base::ASCIIToUTF16("Name");
  ui_data.password_form.password_value = base::ASCIIToUTF16("12345");
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, rect, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  rect = gfx::RectF(200, 30);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, rect, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentTabs) {
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), 20 /*max_length*/, base::ASCIIToUTF16("element"),
      100 /*generation_element_id*/, base::i18n::TextDirection(),
      autofill::PasswordForm());
  ui_data.password_form.username_value = base::ASCIIToUTF16("Name");
  ui_data.password_form.password_value = base::ASCIIToUTF16("12345");
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentDrivers) {
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), 20 /*max_length*/, base::ASCIIToUTF16("element"),
      100 /*generation_element_id*/, base::i18n::TextDirection(),
      autofill::PasswordForm());
  ui_data.password_form.username_value = base::ASCIIToUTF16("Name");
  ui_data.password_form.password_value = base::ASCIIToUTF16("12345");
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  driver = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       GetOrCreateDifferentElements) {
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), 20 /*max_length*/, base::ASCIIToUTF16("element"),
      100 /*generation_element_id*/, base::i18n::TextDirection(),
      autofill::PasswordForm());
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  ui_data.generation_element_id = 200;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

}  // namespace
