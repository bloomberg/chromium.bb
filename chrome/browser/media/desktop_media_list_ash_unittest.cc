// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/desktop_media_list_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

int kThumbnailSize = 100;

using testing::AtLeast;
using testing::DoDefault;

class MockDesktopMediaListObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD1(OnSourceAdded, void(int index));
  MOCK_METHOD1(OnSourceRemoved, void(int index));
  MOCK_METHOD2(OnSourceMoved, void(int old_index, int new_index));
  MOCK_METHOD1(OnSourceNameChanged, void(int index));
  MOCK_METHOD1(OnSourceThumbnailChanged, void(int index));
};

class DesktopMediaListAshTest : public ash::test::AshTestBase {
 public:
  DesktopMediaListAshTest() {}
  virtual ~DesktopMediaListAshTest() {}

  void CreateList(int source_types) {
    list_.reset(new DesktopMediaListAsh(source_types));
    list_->SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    // Set update period to reduce the time it takes to run tests.
    list_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(1));
  }

 protected:
  MockDesktopMediaListObserver observer_;
  scoped_ptr<DesktopMediaListAsh> list_;
  DISALLOW_COPY_AND_ASSIGN(DesktopMediaListAshTest);
};

ACTION(QuitMessageLoop) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitClosure());
}

TEST_F(DesktopMediaListAshTest, Screen) {
  CreateList(DesktopMediaListAsh::SCREENS | DesktopMediaListAsh::WINDOWS);

  EXPECT_CALL(observer_, OnSourceAdded(0));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(QuitMessageLoop())
      .WillRepeatedly(DoDefault());
  list_->StartUpdating(&observer_);
  base::MessageLoop::current()->Run();
}

TEST_F(DesktopMediaListAshTest, OneWindow) {
  CreateList(DesktopMediaListAsh::SCREENS | DesktopMediaListAsh::WINDOWS);

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EXPECT_CALL(observer_, OnSourceAdded(0));
  EXPECT_CALL(observer_, OnSourceAdded(1));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(AtLeast(1));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(1))
      .WillOnce(QuitMessageLoop())
      .WillRepeatedly(DoDefault());
  EXPECT_CALL(observer_, OnSourceRemoved(1))
      .WillOnce(QuitMessageLoop());

  list_->StartUpdating(&observer_);
  base::MessageLoop::current()->Run();
  window.reset();
  base::MessageLoop::current()->Run();
}

TEST_F(DesktopMediaListAshTest, ScreenOnly) {
  CreateList(DesktopMediaListAsh::SCREENS);

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EXPECT_CALL(observer_, OnSourceAdded(0));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(QuitMessageLoop())
      .WillRepeatedly(DoDefault());

  list_->StartUpdating(&observer_);
  base::MessageLoop::current()->Run();
}

TEST_F(DesktopMediaListAshTest, WindowOnly) {
  CreateList(DesktopMediaListAsh::WINDOWS);

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EXPECT_CALL(observer_, OnSourceAdded(0));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(QuitMessageLoop())
      .WillRepeatedly(DoDefault());
  EXPECT_CALL(observer_, OnSourceRemoved(0))
      .WillOnce(QuitMessageLoop());

  list_->StartUpdating(&observer_);
  base::MessageLoop::current()->Run();
  window.reset();
  base::MessageLoop::current()->Run();
}
