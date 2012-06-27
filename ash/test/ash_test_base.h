// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_
#pragma once

#include <string>

#include "ash/shell.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace test {

class AshTestViewsDelegate : public views::TestViewsDelegate {
 public:
  // Overriden from TestViewsDelegate.
  virtual content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) OVERRIDE;
};

class AshTestBase : public testing::Test {
 public:
  AshTestBase();
  virtual ~AshTestBase();

  MessageLoopForUI* message_loop() { return &message_loop_; }

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Change the primary display's configuration to use |bounds|
  // and |scale|.
  void ChangeDisplayConfig(float scale, const gfx::Rect& bounds);

  // Update the display configuration as given in |display_specs|.  The
  // format of |display_spec| is a list of comma separated spec for
  // each displays. Please refer to the comment in
  // | aura::DisplayManager::CreateDisplayFromSpec| for the format of
  // the display spec.
  void UpdateDisplay(const std::string& display_specs);

 protected:
  void RunAllPendingInMessageLoop();

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AshTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
