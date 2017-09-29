// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/ui_pixel_test.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "components/toolbar/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

static const gfx::Transform kIdentity;

}  // namespace

TEST_F(UiPixelTest, DrawVrBrowsingMode) {
  // Set up scene.
  UiInitialState ui_initial_state;
  ui_initial_state.in_cct = false;
  ui_initial_state.in_web_vr = false;
  ui_initial_state.web_vr_autopresentation_expected = false;
  MakeUi(ui_initial_state,
         ToolbarState(GURL("https://example.com"), security_state::SECURE,
                      &toolbar::kHttpsValidIcon, base::UTF8ToUTF16("Secure"),
                      true, false));

  // Draw UI.
  DrawUi(gfx::Vector3dF(0.0f, 0.0f, -1.0f), gfx::Point3F(0.5f, -0.5f, 0.0f),
         UiInputManager::ButtonState::UP, 1.0f, kIdentity, kIdentity,
         kProjMatrix);

  // Read pixels into SkBitmap.
  auto bitmap = SaveCurrentFrameBufferToSkBitmap();
  EXPECT_TRUE(bitmap);
}

}  // namespace vr
