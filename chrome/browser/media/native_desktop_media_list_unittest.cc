// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/native_desktop_media_list.h"

#include "base/message_loop/message_loop.h"
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
  MOCK_METHOD1(OnSourceAdded, void(int index));
  MOCK_METHOD1(OnSourceRemoved, void(int index));
  MOCK_METHOD2(OnSourceMoved, void(int old_index, int new_index));
  MOCK_METHOD1(OnSourceNameChanged, void(int index));
  MOCK_METHOD1(OnSourceThumbnailChanged, void(int index));
};

class FakeScreenCapturer : public webrtc::ScreenCapturer {
 public:
  FakeScreenCapturer() {}
  virtual ~FakeScreenCapturer() {}

  // webrtc::ScreenCapturer implementation.
  virtual void Start(Callback* callback) OVERRIDE {
    callback_ = callback;
  }

  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE {
    DCHECK(callback_);
    webrtc::DesktopFrame* frame =
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10));
    memset(frame->data(), 0, frame->stride() * frame->size().height());
    callback_->OnCaptureCompleted(frame);
  }

  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual bool GetScreenList(ScreenList* screens) OVERRIDE {
    webrtc::ScreenCapturer::Screen screen;
    screen.id = 0;
    screens->push_back(screen);
    return true;
  }

  virtual bool SelectScreen(webrtc::ScreenId id) OVERRIDE {
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
  virtual ~FakeWindowCapturer() {}

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
  virtual void Start(Callback* callback) OVERRIDE {
    callback_ = callback;
  }

  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE {
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

  virtual bool GetWindowList(WindowList* windows) OVERRIDE {
    base::AutoLock lock(window_list_lock_);
    *windows = window_list_;
    return true;
  }

  virtual bool SelectWindow(WindowId id) OVERRIDE {
    selected_window_id_ = id;
    return true;
  }

  virtual bool BringSelectedWindowToFront() OVERRIDE {
    return true;
  }

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
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
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
        EXPECT_CALL(observer_, OnSourceAdded(i))
          .WillOnce(CheckListSize(model_.get(), static_cast<int>(i + 1)));
      }
      for (size_t i = 0; i < source_count - 1; ++i) {
        EXPECT_CALL(observer_, OnSourceThumbnailChanged(i));
      }
      EXPECT_CALL(observer_, OnSourceThumbnailChanged(source_count - 1))
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
    EXPECT_CALL(observer_, OnSourceAdded(0))
      .WillOnce(CheckListSize(model_.get(), 1));
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(QuitMessageLoop(&message_loop_));
  }
  model_->StartUpdating(&observer_);

  message_loop_.Run();

  EXPECT_EQ(model_->GetSource(0).id.type, content::DesktopMediaID::TYPE_SCREEN);
}

TEST_F(DesktopMediaListTest, AddWindow) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(1, false);

  EXPECT_CALL(observer_, OnSourceAdded(2))
    .WillOnce(DoAll(CheckListSize(model_.get(), 3),
                    QuitMessageLoop(&message_loop_)));

  webrtc::WindowCapturer::Window window;
  window.id = 0;
  window.title = "Test window 0";
  list.push_back(window);
  window_capturer_->SetWindowList(list);

  message_loop_.Run();

  EXPECT_EQ(model_->GetSource(2).id.type, content::DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(2).id.id, 0);
}

TEST_F(DesktopMediaListTest, RemoveWindow) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(2, false);

  EXPECT_CALL(observer_, OnSourceRemoved(2))
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
  EXPECT_CALL(observer_, OnSourceRemoved(1))
    .WillOnce(CheckListSize(model_.get(), 2));
  EXPECT_CALL(observer_, OnSourceRemoved(1))
    .WillOnce(DoAll(CheckListSize(model_.get(), 1),
                    QuitMessageLoop(&message_loop_)));

  list.erase(list.begin(), list.end());
  window_capturer_->SetWindowList(list);

  message_loop_.Run();
}

TEST_F(DesktopMediaListTest, UpdateTitle) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(1, false);

  EXPECT_CALL(observer_, OnSourceNameChanged(1))
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

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(1))
    .WillOnce(QuitMessageLoop(&message_loop_));
  // Update frame for the window and verify that we get notification about it.
  window_capturer_->SetNextFrameValue(1, 1);

  message_loop_.Run();
}

TEST_F(DesktopMediaListTest, MoveWindow) {
  CreateWithDefaultCapturers();
  webrtc::WindowCapturer::WindowList list = AddWindowsAndVerify(2, false);

  EXPECT_CALL(observer_, OnSourceMoved(2, 1))
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
