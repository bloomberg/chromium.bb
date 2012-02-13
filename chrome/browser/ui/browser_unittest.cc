// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"

typedef BrowserWithTestWindowTest BrowserTest;

class TestingOffTheRecordDestructionProfile : public TestingProfile {
 public:
  TestingOffTheRecordDestructionProfile() : destroyed_profile_(false) {
    set_incognito(true);
  }
  virtual void DestroyOffTheRecordProfile() OVERRIDE {
    destroyed_profile_ = true;
  }
  bool destroyed_profile_;

  DISALLOW_COPY_AND_ASSIGN(TestingOffTheRecordDestructionProfile);
};

class BrowserTestOffTheRecord : public BrowserTest {
 public:
  BrowserTestOffTheRecord() : off_the_record_profile_(NULL) {}

 protected:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    if (off_the_record_profile_ == NULL)
      off_the_record_profile_ = new TestingOffTheRecordDestructionProfile();
    return off_the_record_profile_;
  }
  TestingOffTheRecordDestructionProfile* off_the_record_profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTestOffTheRecord);
};

// Various assertions around setting show state.
TEST_F(BrowserTest, GetSavedWindowShowState) {
  // Default show state is SHOW_STATE_DEFAULT.
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT, browser()->GetSavedWindowShowState());

  // Explicitly specifying a state should stick though.
  browser()->set_show_state(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, browser()->GetSavedWindowShowState());
  browser()->set_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, browser()->GetSavedWindowShowState());
  browser()->set_show_state(ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED, browser()->GetSavedWindowShowState());
  browser()->set_show_state(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, browser()->GetSavedWindowShowState());
}

TEST_F(BrowserTestOffTheRecord, DelayProfileDestruction) {
  scoped_refptr<content::SiteInstance> instance1(
      static_cast<content::SiteInstance*>(
          content::SiteInstance::Create(off_the_record_profile_)));
  scoped_ptr<content::RenderProcessHost> render_process_host1;
  render_process_host1.reset(instance1->GetProcess());
  ASSERT_TRUE(render_process_host1.get() != NULL);

  scoped_refptr<content::SiteInstance> instance2(
      static_cast<content::SiteInstance*>(
          content::SiteInstance::Create(off_the_record_profile_)));
  scoped_ptr<content::RenderProcessHost> render_process_host2;
  render_process_host2.reset(instance2->GetProcess());
  ASSERT_TRUE(render_process_host2.get() != NULL);

  // destroying the browser should not destroy the off the record profile...
  set_browser(NULL);
  EXPECT_FALSE(off_the_record_profile_->destroyed_profile_);

  // until we destroy the render process host holding on to it...
  render_process_host1.release()->Cleanup();

  // And asynchronicity kicked in properly.
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(off_the_record_profile_->destroyed_profile_);

  // I meant, ALL the render process hosts... :-)
  render_process_host2.release()->Cleanup();
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(off_the_record_profile_->destroyed_profile_);
}
