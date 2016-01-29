// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/native_desktop_media_list.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

using testing::_;
using testing::DoAll;

namespace {

class MockObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD2(OnSourceAdded, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceRemoved, void(DesktopMediaList* list, int index));
  MOCK_METHOD3(OnSourceMoved,
               void(DesktopMediaList* list, int old_index, int new_index));
  MOCK_METHOD2(OnSourceNameChanged, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceThumbnailChanged,
               void(DesktopMediaList* list, int index));
};

class FakeScreenCapturer : public webrtc::ScreenCapturer {
 public:
  FakeScreenCapturer() {}
  ~FakeScreenCapturer() override {}

  // webrtc::ScreenCapturer implementation.
  void Start(Callback* callback) override { callback_ = callback; }

  void Capture(const webrtc::DesktopRegion& region) override {
    DCHECK(callback_);
    webrtc::DesktopFrame* frame =
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10));
    memset(frame->data(), 0, frame->stride() * frame->size().height());
    callback_->OnCaptureCompleted(frame);
  }

  bool GetScreenList(ScreenList* screens) override {
    webrtc::ScreenCapturer::Screen screen;
    screen.id = 0;
    screens->push_back(screen);
    return true;
  }

  bool SelectScreen(webrtc::ScreenId id) override {
    EXPECT_EQ(0, id);
    return true;
  }

 protected:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreenCapturer);
};

class FakeWindowCapturer : public webrtc::WindowCapturer {
 public:
  FakeWindowCapturer()
      : callback_(NULL) {
  }
  ~FakeWindowCapturer() override {}

  void SetWindowList(const WindowList& list) {
    base::AutoLock lock(window_list_lock_);
    window_list_ = list;
  }

  // Sets |value| thats going to be used to memset() content of the frames
  // generated for |window_id|. By default generated frames are set to zeros.
  void SetNextFrameValue(WindowId window_id, int8_t value) {
    base::AutoLock lock(frame_values_lock_);
    frame_values_[window_id] = value;
  }

  // webrtc::WindowCapturer implementation.
  void Start(Callback* callback) override { callback_ = callback; }

  void Capture(const webrtc::DesktopRegion& region) override {
    DCHECK(callback_);

    base::AutoLock lock(frame_values_lock_);

    std::map<WindowId, int8_t>::iterator it =
        frame_values_.find(selected_window_id_);
    int8_t value = (it != frame_values_.end()) ? it->second : 0;
    webrtc::DesktopFrame* frame =
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10));
    memset(frame->data(), value, frame->stride() * frame->size().height());
    callback_->OnCaptureCompleted(frame);
  }

  bool GetWindowList(WindowList* windows) override {
    base::AutoLock lock(window_list_lock_);
    *windows = window_list_;
    return true;
  }

  bool SelectWindow(WindowId id) override {
    selected_window_id_ = id;
    return true;
  }

  bool BringSelectedWindowToFront() override { return true; }

 private:
  Callback* callback_;
  WindowList window_list_;
  base::Lock window_list_lock_;

  WindowId selected_window_id_;

  // Frames to be captured per window.
  std::map<WindowId, int8_t> frame_values_;
  base::Lock frame_values_lock_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowCapturer);
};

ACTION_P2(CheckListSize, model, expected_list_size) {
  EXPECT_EQ(expected_list_size, model->GetSourceCount());
}

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

class DesktopMediaListTest : public testing::Test {
 public:
  DesktopMediaListTest()
      : window_capturer_(NULL),
        ui_thread_(content::BrowserThread::UI,
                   &message_loop_) {
  }

  void CreateWithDefaultCapturers() {
    window_capturer_ = new FakeWindowCapturer();
    model_.reset(new NativeDesktopMediaList(
        scoped_ptr<webrtc::ScreenCapturer>(new FakeScreenCapturer()),
        scoped_ptr<webrtc::WindowCapturer>(window_capturer_)));

    // Set update period to reduce the time it takes to run tests.
    model_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(0));
  }

  webrtc::WindowCapturer::WindowList AddWindowsAndVerify(
      size_t count, bool window_only) {
    webrtc::WindowCapturer::WindowList list;
    for (size_t i = 0; i < count; ++i) {
      webrtc::WindowCapturer::Window window;
      window.id = i + 1;
      window.title = "Test window";
      list.push_back(window);
    }
    window_capturer_->SetWindowList(list);

    {
      testing::InSequence dummy;
      size_t source_count = window_only ? count : count + 1;
      for (size_t i = 0; i < source_count; ++i) {
        EXPECT_CALL(observer_, OnSourceAdded(model_.get(), i))
            .WillOnce(CheckListSize(model_.get(), static_cast<int>(i + 1)));
      }
      for (size_t i = 0; i < source_count - 1; ++i) {
        EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), i));
      }
      EXPECT_CALL(observer_,
                  OnSourceThumbnailChanged(model_.get(), source_count - 1))
          .WillOnce(QuitMessageLoop(&message_loop_));
    }
    model_->StartUpdating(&observer_);
    message_loop_.Run();

    for (size_t i = 0; i < count; ++i) {
      size_t source_index = window_only ? i : i + 1;
      EXPECT_EQ(model_->GetSource(source_index).id.type,
                                  content::DesktopMediaID::TYPE_WINDOW);
      EXPECT_EQ(model_->GetSource(source_index).id.id, static_cast<int>(i + 1));
      EXPECT_EQ(model_->GetSource(source_index).name,
                                  base::UTF8ToUTF16("Test window"));
    }
    testing::Mock::VerifyAndClearExpectations(&observer_);
    return list;
  }

 protected:
  // Must be listed before |model_|, so it's destroyed last.
  MockObserver observer_;

  // Owned by |model_|;
  FakeWindowCapturer* window_capturer_;

  scoped_ptr<NativeDesktopMediaList> model_;

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaListTest);
};

TEST_F(DesktopMediaListTest, InitialSourceList) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(1, false);

  EXPECT_EQ(model_->GetSource(0).id.type, content::DesktopMediaID::TYPE_SCREEN);
  EXPECT_EQ(model_->GetSource(0).id.id, 0);
}

// Verifies that the window specified with SetViewDialogWindowId() is filtered
// from the results.
TEST_F(DesktopMediaListTest, Filtering) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(2, false);

  EXPECT_EQ(model_->GetSource(0).id.type, content::DesktopMediaID::TYPE_SCREEN);
  EXPECT_EQ(model_->GetSource(0).id.id, 0);
}

TEST_F(DesktopMediaListTest, WindowsOnly) {
  window_capturer_ = new FakeWindowCapturer();
  model_.reset(new NativeDesktopMediaList(
      scoped_ptr<webrtc::ScreenCapturer>(),
      scoped_ptr<webrtc::WindowCapturer>(window_capturer_)));
  AddWindowsAndVerify(1, true);
}

TEST_F(DesktopMediaListTest, ScreenOnly) {
  model_.reset(new NativeDesktopMediaList(
      scoped_ptr<webrtc::ScreenCapturer>(new FakeScreenCapturer),
      scoped_ptr<webrtc::WindowCapturer>()));

  {
    testing::InSequence dummy;
    EXPECT_CALL(observer_, OnSourceAdded(model_.get(), 0))
        .WillOnce(CheckListSize(model_.get(), 1));
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), 0))
        .WillOnce(QuitMessageLoop(&message_loop_));
  }
  model_->StartUpdating(&observer_);

  message_loop_.Run();

  EXPECT_EQ(model_->GetSource(0).id.type, content::DesktopMediaID::TYPE_SCREEN);
}

TEST_F(DesktopMediaListTest, AddWindow) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(1, false);

  EXPECT_CALL(observer_, OnSourceAdded(model_.get(), 2))
      .WillOnce(DoAll(CheckListSize(model_.get(), 3),
                      QuitMessageLoop(&message_loop_)));

  webrtc::WindowCapturer::Window window;
  window.id = 10;  // id=0 is invalid.
  window.title = "Test window 10";
  list.push_back(window);
  window_capturer_->SetWindowList(list);

  message_loop_.Run();

  EXPECT_EQ(model_->GetSource(2).id.type, content::DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(2).id.id, 10);
}

TEST_F(DesktopMediaListTest, RemoveWindow) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(2, false);

  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 2))
      .WillOnce(DoAll(CheckListSize(model_.get(), 2),
                      QuitMessageLoop(&message_loop_)));

  list.erase(list.begin() + 1);
  window_capturer_->SetWindowList(list);

  message_loop_.Run();
}

TEST_F(DesktopMediaListTest, RemoveAllWindows) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(2, false);

  testing::InSequence seq;
  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 1))
      .WillOnce(CheckListSize(model_.get(), 2));
  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 1))
      .WillOnce(DoAll(CheckListSize(model_.get(), 1),
                      QuitMessageLoop(&message_loop_)));

  list.erase(list.begin(), list.end());
  window_capturer_->SetWindowList(list);

  message_loop_.Run();
}

TEST_F(DesktopMediaListTest, UpdateTitle) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(1, false);

  EXPECT_CALL(observer_, OnSourceNameChanged(model_.get(), 1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  const std::string kTestTitle = "New Title";

  list[0].title = kTestTitle;
  window_capturer_->SetWindowList(list);

  message_loop_.Run();

  EXPECT_EQ(model_->GetSource(1).name, base::UTF8ToUTF16(kTestTitle));
}

TEST_F(DesktopMediaListTest, UpdateThumbnail) {
  CreateWithDefaultCapturers();
  AddWindowsAndVerify(2, false);

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), 1))
      .WillOnce(QuitMessageLoop(&message_loop_));
  // Update frame for the window and verify that we get notification about it.
  window_capturer_->SetNextFrameValue(1, 1);

  message_loop_.Run();
}

TEST_F(DesktopMediaListTest, MoveWindow) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(2, false);

  EXPECT_CALL(observer_, OnSourceMoved(model_.get(), 2, 1))
      .WillOnce(DoAll(CheckListSize(model_.get(), 3),
                      QuitMessageLoop(&message_loop_)));

  // Swap the two windows.
  webrtc::WindowCapturer::Window temp = list[0];
  list[0] = list[1];
  list[1] = temp;
  window_capturer_->SetWindowList(list);

  message_loop_.Run();
}

}  // namespace
