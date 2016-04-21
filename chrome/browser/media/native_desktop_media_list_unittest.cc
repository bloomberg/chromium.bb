// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/native_desktop_media_list.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
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
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

using content::DesktopMediaID;
using testing::_;
using testing::DoAll;

namespace {

// Aura window capture unit tests are not stable in linux. crbug.com/602494 and
// crbug.com/603823.
#if defined(OS_WIN)
#define ENABLE_AURA_WINDOW_TESTS
#endif

static const int kDefaultWindowCount = 2;
#if defined(ENABLE_AURA_WINDOW_TESTS)
static const int kDefaultAuraCount = 1;
#else
static const int kDefaultAuraCount = 0;
#endif

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

}  // namespace

ACTION_P2(CheckListSize, model, expected_list_size) {
  EXPECT_EQ(expected_list_size, model->GetSourceCount());
}

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

class NativeDesktopMediaListTest : public views::ViewsTestBase {
 public:
  NativeDesktopMediaListTest()
      : ui_thread_(content::BrowserThread::UI, message_loop()) {}

  void TearDown() override {
    for (size_t i = 0; i < desktop_widgets_.size(); i++)
      desktop_widgets_[i].reset();

    ViewsTestBase::TearDown();
  }

  void CreateWithCapturers(bool screen, bool window) {
    webrtc::ScreenCapturer* screen_capturer = nullptr;
    if (screen)
      screen_capturer = new FakeScreenCapturer();

    if (window)
      window_capturer_ = new FakeWindowCapturer();
    else
      window_capturer_ = nullptr;

    model_.reset(new NativeDesktopMediaList(
        std::unique_ptr<webrtc::ScreenCapturer>(screen_capturer),
        std::unique_ptr<webrtc::WindowCapturer>(window_capturer_)));

    // Set update period to reduce the time it takes to run tests.
    model_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(20));
  }

  void AddNativeWindow(int id) {
    webrtc::WindowCapturer::Window window;
    window.id = id;
    window.title = "Test window";
    window_list_.push_back(window);
  }

#if defined(USE_AURA)
  std::unique_ptr<views::Widget> CreateDesktopWidget() {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.accept_events = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new views::DesktopNativeWidgetAura(widget.get());
    params.bounds = gfx::Rect(0, 0, 20, 20);
    widget->Init(params);
    widget->Show();
    return widget;
  }

  void AddAuraWindow() {
    webrtc::WindowCapturer::Window window;
    window.title = "Test window";
    // Create a aura native widow through a widget.
    desktop_widgets_.push_back(CreateDesktopWidget());
    // Get the native window's id.
    aura::Window* aura_window = desktop_widgets_.back()->GetNativeWindow();
    gfx::AcceleratedWidget widget =
        aura_window->GetHost()->GetAcceleratedWidget();
#if defined(OS_WIN)
    window.id = reinterpret_cast<DesktopMediaID::Id>(widget);
#else
    window.id = widget;
#endif
    // Get the aura window's id.
    DesktopMediaID aura_id = DesktopMediaID::RegisterAuraWindow(
        DesktopMediaID::TYPE_WINDOW, aura_window);
    native_aura_id_map_[window.id] = aura_id.aura_id;

    window_list_.push_back(window);
  }

  void RemoveAuraWindow(int index) {
    DCHECK_LT(index, static_cast<int>(desktop_widgets_.size()));

    // Get the native window's id.
    aura::Window* aura_window = desktop_widgets_[index]->GetNativeWindow();
    gfx::AcceleratedWidget widget =
        aura_window->GetHost()->GetAcceleratedWidget();
#if defined(OS_WIN)
    int native_id = reinterpret_cast<DesktopMediaID::Id>(widget);
#else
    int native_id = widget;
#endif
    // Remove the widget and assoicate aura window.
    desktop_widgets_.erase(desktop_widgets_.begin() + index);
    // Remove the aura window from the window list.
    size_t i;
    for (i = 0; i < window_list_.size(); i++) {
      if (window_list_[i].id == native_id)
        break;
    }
    DCHECK_LT(i, window_list_.size());
    window_list_.erase(window_list_.begin() + i);
    native_aura_id_map_.erase(native_id);
  }

#endif  // defined(USE_AURA)

  void AddWindowsAndVerify(bool screen,
                           size_t window_count,
                           size_t aura_count,
                           bool has_view_dialog) {
    // Create model_.
    CreateWithCapturers(screen, window_count > 0);

#if !defined(USE_AURA)
    aura_count = 0;
#endif
    if (aura_count >= window_count)
      aura_count = window_count - 1;

    if (window_count == 0)
      has_view_dialog = false;

    // Set up widows.
    size_t aura_window_first_index = window_count - aura_count;
    for (size_t i = 0; i < window_count; ++i) {
      if (i < aura_window_first_index) {
        AddNativeWindow(i + 1);
      } else {
#if defined(USE_AURA)
        AddAuraWindow();
#endif
      }
    }

    if (window_capturer_)
      window_capturer_->SetWindowList(window_list_);

    // Set view dialog window ID as the first window id.
    if (has_view_dialog) {
      DesktopMediaID dialog_window_id(DesktopMediaID::TYPE_WINDOW,
                                      window_list_[0].id);
      model_->SetViewDialogWindowId(dialog_window_id);
      window_count--;
      aura_window_first_index--;
    }

    {
      testing::InSequence dummy;
      size_t source_count = screen ? window_count + 1 : window_count;
      for (size_t i = 0; i < source_count; ++i) {
        EXPECT_CALL(observer_, OnSourceAdded(model_.get(), i))
            .WillOnce(CheckListSize(model_.get(), static_cast<int>(i + 1)));
      }
      for (size_t i = 0; i < source_count - 1; ++i) {
        EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), i));
      }
      EXPECT_CALL(observer_,
                  OnSourceThumbnailChanged(model_.get(), source_count - 1))
          .WillOnce(QuitMessageLoop(message_loop()));
    }
    model_->StartUpdating(&observer_);
    message_loop()->Run();

    if (screen) {
      EXPECT_EQ(model_->GetSource(0).id.type, DesktopMediaID::TYPE_SCREEN);
      EXPECT_EQ(model_->GetSource(0).id.id, 0);
    }

    for (size_t i = 0; i < window_count; ++i) {
      size_t source_index = screen ? i + 1 : i;
      EXPECT_EQ(model_->GetSource(source_index).id.type,
                DesktopMediaID::TYPE_WINDOW);
      EXPECT_EQ(model_->GetSource(source_index).name,
                base::UTF8ToUTF16("Test window"));
      int index = has_view_dialog ? i + 1 : i;
      int native_id = window_list_[index].id;
      EXPECT_EQ(model_->GetSource(source_index).id.id, native_id);
#if defined(USE_AURA)
      if (i >= aura_window_first_index)
        EXPECT_EQ(model_->GetSource(source_index).id.aura_id,
                  native_aura_id_map_[native_id]);
#endif
    }
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }

 protected:
  // Must be listed before |model_|, so it's destroyed last.
  MockObserver observer_;

  // Owned by |model_|;
  FakeWindowCapturer* window_capturer_;

  webrtc::WindowCapturer::WindowList window_list_;
  std::vector<std::unique_ptr<views::Widget>> desktop_widgets_;
  std::map<DesktopMediaID::Id, DesktopMediaID::Id> native_aura_id_map_;
  std::unique_ptr<NativeDesktopMediaList> model_;

  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(NativeDesktopMediaListTest);
};

TEST_F(NativeDesktopMediaListTest, WindowsOnly) {
  AddWindowsAndVerify(false, kDefaultWindowCount, kDefaultAuraCount, false);
}

TEST_F(NativeDesktopMediaListTest, ScreenOnly) {
  AddWindowsAndVerify(true, 0, 0, false);
}

// Verifies that the window specified with SetViewDialogWindowId() is filtered
// from the results.
TEST_F(NativeDesktopMediaListTest, Filtering) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, true);
}

TEST_F(NativeDesktopMediaListTest, AddNativeWindow) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  const int index = kDefaultWindowCount + 1;
  EXPECT_CALL(observer_, OnSourceAdded(model_.get(), index))
      .WillOnce(DoAll(CheckListSize(model_.get(), index + 1),
                      QuitMessageLoop(message_loop())));

  webrtc::WindowCapturer::Window window;
  AddNativeWindow(index);
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();

  EXPECT_EQ(model_->GetSource(index).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(index).id.id, index);
}

#if defined(ENABLE_AURA_WINDOW_TESTS)
TEST_F(NativeDesktopMediaListTest, AddAuraWindow) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  const int index = kDefaultWindowCount + 1;
  EXPECT_CALL(observer_, OnSourceAdded(model_.get(), index))
      .WillOnce(DoAll(CheckListSize(model_.get(), index + 1),
                      QuitMessageLoop(message_loop())));

  AddAuraWindow();
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();

  int native_id = window_list_.back().id;
  EXPECT_EQ(model_->GetSource(index).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(index).id.id, native_id);
  EXPECT_EQ(model_->GetSource(index).id.aura_id,
            native_aura_id_map_[native_id]);
}
#endif  // defined(ENABLE_AURA_WINDOW_TESTS)

TEST_F(NativeDesktopMediaListTest, RemoveNativeWindow) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 1))
      .WillOnce(DoAll(CheckListSize(model_.get(), kDefaultWindowCount),
                      QuitMessageLoop(message_loop())));

  window_list_.erase(window_list_.begin());
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();
}

#if defined(ENABLE_AURA_WINDOW_TESTS)
TEST_F(NativeDesktopMediaListTest, RemoveAuraWindow) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  int aura_window_start_index = kDefaultWindowCount - kDefaultAuraCount + 1;
  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), aura_window_start_index))
      .WillOnce(DoAll(CheckListSize(model_.get(), kDefaultWindowCount),
                      QuitMessageLoop(message_loop())));

  RemoveAuraWindow(0);
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();
}
#endif  // defined(ENABLE_AURA_WINDOW_TESTS)

TEST_F(NativeDesktopMediaListTest, RemoveAllWindows) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  testing::InSequence seq;
  for (int i = 0; i < kDefaultWindowCount - 1; i++) {
    EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 1))
        .WillOnce(CheckListSize(model_.get(), kDefaultWindowCount - i));
  }
  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 1))
      .WillOnce(DoAll(CheckListSize(model_.get(), 1),
                      QuitMessageLoop(message_loop())));

  window_list_.clear();
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();
}

TEST_F(NativeDesktopMediaListTest, UpdateTitle) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  EXPECT_CALL(observer_, OnSourceNameChanged(model_.get(), 1))
      .WillOnce(QuitMessageLoop(message_loop()));

  const std::string kTestTitle = "New Title";
  window_list_[0].title = kTestTitle;
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();

  EXPECT_EQ(model_->GetSource(1).name, base::UTF8ToUTF16(kTestTitle));
}

TEST_F(NativeDesktopMediaListTest, UpdateThumbnail) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), 1))
      .WillOnce(QuitMessageLoop(message_loop()));

  // Update frame for the window and verify that we get notification about it.
  window_capturer_->SetNextFrameValue(1, 10);

  message_loop()->Run();
}

TEST_F(NativeDesktopMediaListTest, MoveWindow) {
  AddWindowsAndVerify(true, kDefaultWindowCount, kDefaultAuraCount, false);

  EXPECT_CALL(observer_, OnSourceMoved(model_.get(), 2, 1))
      .WillOnce(DoAll(CheckListSize(model_.get(), kDefaultWindowCount + 1),
                      QuitMessageLoop(message_loop())));

  // Swap the two windows.
  webrtc::WindowCapturer::Window temp = window_list_[0];
  window_list_[0] = window_list_[1];
  window_list_[1] = temp;
  window_capturer_->SetWindowList(window_list_);

  message_loop()->Run();
}
