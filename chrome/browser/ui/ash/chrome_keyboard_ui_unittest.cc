// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"

#include "base/macros.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace {

class TestChromeKeyboardUI : public ChromeKeyboardUI {
 public:
  explicit TestChromeKeyboardUI(std::unique_ptr<content::WebContents> contents)
      : ChromeKeyboardUI(contents->GetBrowserContext()),
        contents_(std::move(contents)) {}
  ~TestChromeKeyboardUI() override {}

  ui::InputMethod* GetInputMethod() override { return nullptr; }
  void RequestAudioInput(content::WebContents* web_contents,
                         const content::MediaStreamRequest& request,
                         content::MediaResponseCallback callback) {}

  std::unique_ptr<content::WebContents> CreateWebContents() override {
    return std::move(contents_);
  }

 private:
  std::unique_ptr<content::WebContents> contents_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeKeyboardUI);
};

}  // namespace

using ChromeKeyboardUITest = ChromeRenderViewHostTestHarness;
