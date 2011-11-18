// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlwin.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "chrome_frame/infobars/infobar_content.h"
#include "chrome_frame/infobars/internal/displaced_window_manager.h"
#include "chrome_frame/infobars/internal/host_window_manager.h"
#include "chrome_frame/infobars/internal/infobar_window.h"
#include "chrome_frame/infobars/internal/subclassing_window_with_delegate.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"

namespace {

RECT kInitialParentWindowRect = {20, 20, 300, 300};
RECT kInitialChildWindowRect = {20, 20, 280, 280};

MATCHER_P(EqualRect, expected, "") {
  return ::EqualRect(expected, arg);
}

ACTION_P2(RespondToNcCalcSize, result, rect) {
  *reinterpret_cast<RECT*>(arg1) = *rect;
  return result;
}

ACTION_P4(RespondToNcCalcSize, result, rect1, rect2, rect3) {
  reinterpret_cast<RECT*>(arg1)[0] = rect1;
  reinterpret_cast<RECT*>(arg1)[1] = rect2;
  reinterpret_cast<RECT*>(arg1)[2] = rect3;
  return result;
}

class ParentTraits : public CFrameWinTraits {
 public:
  static const wchar_t* kClassName;
};  // class ParentTraits

class ChildTraits : public CControlWinTraits {
 public:
  static const wchar_t* kClassName;
};  // class ChildTraits

const wchar_t* ParentTraits::kClassName = NULL;
const wchar_t* ChildTraits::kClassName = L"Shell DocObject View";

template<typename TRAITS> class MockWindow
    : public CWindowImpl<MockWindow<TRAITS>, CWindow, TRAITS> {
 public:
  virtual ~MockWindow() {
    if (IsWindow())
      DestroyWindow();
  }
  MOCK_METHOD1(OnCreate, int(LPCREATESTRUCT lpCreateStruct));
  MOCK_METHOD0(OnDestroy, void());
  MOCK_METHOD2(OnSize, void(UINT nType, CSize size));
  MOCK_METHOD1(OnMove, void(CPoint ptPos));
  MOCK_METHOD2(OnNcCalcSize, LRESULT(BOOL bCalcValidRects, LPARAM lParam));
  DECLARE_WND_CLASS(TRAITS::kClassName);
  BEGIN_MSG_MAP_EX(MockWindow)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_SIZE(OnSize)
    MSG_WM_MOVE(OnMove)
    MSG_WM_NCCALCSIZE(OnNcCalcSize)
  END_MSG_MAP()
};  // class MockWindow

typedef MockWindow<ParentTraits> MockTopLevelWindow;
typedef MockWindow<ChildTraits> MockChildWindow;

class MockWindowSubclass
    : public SubclassingWindowWithDelegate<MockWindowSubclass> {
 public:
  MOCK_METHOD2(OnNcCalcSize, LRESULT(BOOL bCalcValidRects, LPARAM lParam));
  BEGIN_MSG_MAP_EX(MockWindowSubclass)
    MSG_WM_NCCALCSIZE(OnNcCalcSize)
    CHAIN_MSG_MAP(SubclassingWindowWithDelegate<MockWindowSubclass>)
  END_MSG_MAP()
  virtual ~MockWindowSubclass() { Die(); }
  MOCK_METHOD0(Die, void());
};  // class MockWindowSubclass

template<typename T> class MockDelegate
    : public SubclassingWindowWithDelegate<T>::Delegate {
 public:
  virtual ~MockDelegate() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_METHOD1(AdjustDisplacedWindowDimensions, void(RECT* rect));
};  // clas MockDelegate

template<typename T> T* Initialize(T* t,
                                   HWND hwnd,
                                   typename T::Delegate* delegate) {
  if (t->Initialize(hwnd, delegate)) {
    return t;
  } else {
    delete t;
    return NULL;
  }
}

};  // namespace

TEST(InfobarsSubclassingWindowWithDelegateTest, BasicTest) {
  testing::NiceMock<MockTopLevelWindow> window;
  ASSERT_TRUE(window.Create(NULL, kInitialParentWindowRect) != NULL);

  HWND hwnd = static_cast<HWND>(window);

  ASSERT_TRUE(MockWindowSubclass::GetDelegateForHwnd(hwnd) == NULL);

  MockDelegate<MockWindowSubclass>* delegate =
      new testing::StrictMock<MockDelegate<MockWindowSubclass> >();
  MockWindowSubclass* swwd = Initialize(
      new testing::StrictMock<MockWindowSubclass>(), hwnd, delegate);
  ASSERT_TRUE(swwd != NULL);
  ASSERT_EQ(static_cast<MockWindowSubclass::Delegate*>(delegate),
            MockWindowSubclass::GetDelegateForHwnd(hwnd));

  // Since expectations are only validated upon object destruction, this test
  // would normally pass even if the expected deletions never happened.
  // By expecting another call, in sequence, after the deletions, we force a
  // failure if those deletions don't occur.
  testing::MockFunction<void(std::string check_point_name)> check;

  {
    testing::InSequence s;
    EXPECT_CALL(*delegate, Die());
    EXPECT_CALL(*swwd, Die());
    EXPECT_CALL(check, Call("checkpoint"));
  }

  window.DestroyWindow();

  check.Call("checkpoint");

  ASSERT_TRUE(MockWindowSubclass::GetDelegateForHwnd(hwnd) == NULL);
}

TEST(InfobarsSubclassingWindowWithDelegateTest, InvalidHwndTest) {
  testing::NiceMock<MockTopLevelWindow> window;
  ASSERT_TRUE(window.Create(NULL, kInitialParentWindowRect) != NULL);

  HWND hwnd = static_cast<HWND>(window);

  window.DestroyWindow();

  MockDelegate<MockWindowSubclass>* delegate =
      new testing::StrictMock<MockDelegate<MockWindowSubclass> >();
  MockWindowSubclass* swwd = new testing::StrictMock<MockWindowSubclass>();

  testing::MockFunction<void(std::string check_point_name)> check;

  {
    testing::InSequence s;
    EXPECT_CALL(*delegate, Die());
    EXPECT_CALL(check, Call("checkpoint"));
    EXPECT_CALL(*swwd, Die());
  }

  ASSERT_FALSE(swwd->Initialize(hwnd, delegate));
  check.Call("checkpoint");  // Make sure the delegate has been deleted
  delete swwd;

  ASSERT_TRUE(MockWindowSubclass::GetDelegateForHwnd(hwnd) == NULL);
}

template <typename WINDOW, typename DELEGATE> void ExpectNcCalcSizeSequence(
    WINDOW* mock_window,
    DELEGATE* delegate,
    RECT* natural_rect,
    RECT* modified_rect) {
  testing::InSequence s;
  EXPECT_CALL(*mock_window, OnNcCalcSize(true, testing::_)).WillOnce(
      RespondToNcCalcSize(0, natural_rect));
  EXPECT_CALL(*delegate,
              AdjustDisplacedWindowDimensions(EqualRect(natural_rect)))
      .WillOnce(testing::SetArgumentPointee<0>(*modified_rect));
  EXPECT_CALL(*mock_window, OnMove(CPoint(modified_rect->left,
                                          modified_rect->top)));
  EXPECT_CALL(*mock_window,
              OnSize(0, CSize(modified_rect->right - modified_rect->left,
                              modified_rect->bottom - modified_rect->top)));

  EXPECT_CALL(*mock_window, OnNcCalcSize(true, testing::_))
    .Times(testing::Between(0, 1))
    .WillOnce(RespondToNcCalcSize(0, natural_rect));
  EXPECT_CALL(*delegate,
              AdjustDisplacedWindowDimensions(EqualRect(natural_rect)))
    .Times(testing::Between(0, 1))
    .WillOnce(testing::SetArgumentPointee<0>(*modified_rect));
  EXPECT_CALL(*mock_window, OnMove(CPoint(modified_rect->left,
                                          modified_rect->top)))
    .Times(testing::Between(0, 1));
  EXPECT_CALL(*mock_window,
              OnSize(0, CSize(modified_rect->right - modified_rect->left,
                              modified_rect->bottom - modified_rect->top)))
    .Times(testing::Between(0, 1));
}

template <typename WINDOW, typename DELEGATE, typename MANAGER>
    void DoNcCalcSizeSequence(WINDOW* mock_window,
                              DELEGATE* delegate,
                              MANAGER* manager) {
  RECT natural_rects[] = { {0, 0, 100, 100}, {10, 10, 120, 120} };
  RECT modified_rects[] = { {10, 5, 90, 95}, {25, 35, 80, 70} };

  ExpectNcCalcSizeSequence(
      mock_window, delegate, &natural_rects[0], &natural_rects[0]);
  // The first time through, trigger the sizing via the manager.
  // This is required for the HostWindowManager, since it only looks up
  // and subclasses the displaced window on demand.
  manager->UpdateLayout();

  testing::Mock::VerifyAndClearExpectations(mock_window);
  testing::Mock::VerifyAndClearExpectations(delegate);

  ExpectNcCalcSizeSequence(
      mock_window, delegate, &natural_rects[1], &natural_rects[1]);
  // The second time through, trigger it through the original window.
  // By now, we expect to be observing the window in all cases.
  ::SetWindowPos(static_cast<HWND>(*mock_window),
                 NULL, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_FRAMECHANGED);

  testing::Mock::VerifyAndClearExpectations(mock_window);
  testing::Mock::VerifyAndClearExpectations(delegate);
}

TEST(InfobarsDisplacedWindowManagerTest, BasicTest) {
  testing::NiceMock<MockTopLevelWindow> window;
  ASSERT_TRUE(window.Create(NULL, kInitialParentWindowRect) != NULL);

  MockDelegate<DisplacedWindowManager>* delegate =
      new testing::StrictMock<MockDelegate<DisplacedWindowManager> >();

  DisplacedWindowManager* dwm = new DisplacedWindowManager();
  ASSERT_TRUE(dwm->Initialize(static_cast<HWND>(window), delegate));
  ASSERT_NO_FATAL_FAILURE(DoNcCalcSizeSequence(&window, delegate, dwm));

  EXPECT_CALL(*delegate, Die());
  window.DestroyWindow();
}

TEST(InfobarsHostWindowManagerTest, BasicTest) {
  testing::NiceMock<MockTopLevelWindow> window;
  ASSERT_TRUE(window.Create(NULL, kInitialParentWindowRect) != NULL);
  testing::NiceMock<MockChildWindow> child_window;
  ASSERT_TRUE(child_window.Create(window, kInitialChildWindowRect) != NULL);
  testing::NiceMock<MockChildWindow> child_window2;
  testing::NiceMock<MockChildWindow> child_window3;

  MockDelegate<HostWindowManager>* delegate =
      new testing::StrictMock<MockDelegate<HostWindowManager> >();

  HostWindowManager* hwm = new HostWindowManager();

  ASSERT_TRUE(hwm->Initialize(static_cast<HWND>(window), delegate));
  ASSERT_NO_FATAL_FAILURE(DoNcCalcSizeSequence(&child_window, delegate, hwm));

  // First, destroy window 1 and subsequently create window 2
  child_window.DestroyWindow();

  ASSERT_TRUE(child_window2.Create(window, kInitialChildWindowRect) != NULL);
  ASSERT_NO_FATAL_FAILURE(DoNcCalcSizeSequence(&child_window2, delegate, hwm));

  // Next, create window 3 just before destroying window 2
  RECT natural_rect = {10, 15, 40, 45};
  RECT modified_rect = {15, 20, 35, 40};

  ASSERT_TRUE(child_window3.Create(window, kInitialChildWindowRect) != NULL);
  ExpectNcCalcSizeSequence(
      &child_window3, delegate, &natural_rect, &natural_rect);
  child_window2.DestroyWindow();

  ASSERT_NO_FATAL_FAILURE(DoNcCalcSizeSequence(&child_window3, delegate, hwm));

  EXPECT_CALL(*delegate, Die());
  window.DestroyWindow();
}

// Must be declared in same namespace as RECT
void PrintTo(const RECT& rect, ::std::ostream* os) {
  *os << "{" << rect.left << ", " << rect.top << ", " << rect.right << ", "
      << rect.bottom << "}";
}

namespace {

class MockHost : public InfobarWindow::Host {
 public:
  MockHost(InfobarWindow* infobar_window,
           const RECT& natural_dimensions,
           HWND hwnd)
      : infobar_window_(infobar_window),
        natural_dimensions_(natural_dimensions),
        hwnd_(hwnd) {
  }

  void SetNaturalDimensions(const RECT& natural_dimensions) {
    natural_dimensions_ = natural_dimensions;
    UpdateLayout();
  }

  virtual HWND GetContainerWindow() {
    return hwnd_;
  }

  virtual void UpdateLayout() {
    RECT temp(natural_dimensions_);
    infobar_window_->ReserveSpace(&temp);
    CheckReservedSpace(&temp);
  }

  MOCK_METHOD0(Die, void(void));

  // Convenience method for checking the result of InfobarWindow::ReserveSpace
  MOCK_METHOD1(CheckReservedSpace, void(RECT* rect));

 private:
  InfobarWindow* infobar_window_;
  RECT natural_dimensions_;
  HWND hwnd_;
};  // class MockHost

class MockInfobarContent : public InfobarContent {
 public:
  virtual ~MockInfobarContent() { Die(); }

  MOCK_METHOD1(InstallInFrame,  bool(Frame* frame));
  MOCK_METHOD1(SetDimensions, void(const RECT& dimensions));
  MOCK_METHOD2(GetDesiredSize, size_t(size_t width, size_t height));
  MOCK_METHOD0(Die, void(void));
};  // class MockInfobarContent

MATCHER_P(
    RectHeightIsLTEToRectXHeight, rect_x, "height is <= to height of rect x") {
  return arg.bottom - arg.top <= rect_x->bottom - rect_x->top;
}

MATCHER_P(
    RectHeightIsGTEToRectXHeight, rect_x, "height is >= to height of rect x") {
  return arg.bottom - arg.top >= rect_x->bottom - rect_x->top;
}

MATCHER_P3(RectIsTopXToYOfRectZ, x, y, rect_z, "is top x to y of rect z" ) {
  return rect_z->top == arg.top &&
      rect_z->left == arg.left &&
      rect_z->right == arg.right &&
      arg.bottom - arg.top >= x &&
      arg.bottom - arg.top <= y;
}

MATCHER_P2(RectAddedToRectXMakesRectY,
           rect_x,
           rect_y,
           "rect added to rect x makes rect y") {
  if (::IsRectEmpty(rect_x))
    return ::EqualRect(arg, rect_y);

  if (::IsRectEmpty(arg))
    return ::EqualRect(rect_x, rect_y);

  // Either they are left and right slices, or top and bottom slices
  if (!((rect_x->left == rect_y->left && rect_x->right == rect_y->right &&
         arg->left == rect_y->left && arg->right == rect_y->right) ||
        (rect_x->top == rect_y->top && rect_x->bottom== rect_y->bottom &&
         arg->top == rect_y->top && arg->bottom == rect_y->bottom))) {
    return false;
  }

  RECT expected_arg;

  if (!::SubtractRect(&expected_arg, rect_y, rect_x))
    return false;  // Given above checks, the difference should not be empty

  return ::EqualRect(arg, &expected_arg);
}

MATCHER_P(FrameHwndIs, hwnd, "") {
  return arg != NULL && arg->GetFrameWindow() == hwnd;
}

ACTION_P(CheckSetFlag, flag) {
  ASSERT_FALSE(*flag);
  *flag = true;
}

ACTION_P(ResetFlag, flag) {
  *flag = false;
}

ACTION_P2(AsynchronousCloseOnFrame, loop, frame) {
  loop->PostTask(FROM_HERE, base::Bind(&InfobarContent::Frame::CloseInfobar,
                                       base::Unretained(*frame)));
}

ACTION_P2(AsynchronousHideOnManager, loop, manager) {
  loop->PostTask(FROM_HERE, base::Bind(&InfobarManager::Hide,
                                       base::Unretained(manager), TOP_INFOBAR));
}

};  // namespace

// The test ensures that the content is sized at least once in each of the
// following ranges while fully opening and closing:
//
// [0, infobar_height / 2)
// [infobar_height / 2, infobar_height)
// [infobar_height, infobar_height]
// (infobar_height / 2, infobar_height]
// [0, infobar_height / 2]
//
// If the test turns out to be flaky (i.e., because timers are not firing
// frequently enough to hit all the ranges), increasing the infobar_height
// should increase the margin (by increasing the time spent in each range).
TEST(InfobarsInfobarWindowTest, SlidingTest) {
  int infobar_height = 40;

  chrome_frame_test::TimedMsgLoop message_loop;

  RECT natural_dimensions = {10, 20, 90, 100 + infobar_height};

  // Used to verify that the last RECT given to SetDimensions is the same RECT
  // reserved by ReserveSpace.
  RECT current_infobar_dimensions = {0, 0, 0, 0};

  // Used to make sure that each SetDimensions is matched by a return from
  // ReserveSpace.
  bool pending_reserve_space = false;

  InfobarWindow infobar_window(TOP_INFOBAR);

  testing::NiceMock<MockTopLevelWindow> window;
  ASSERT_TRUE(window.Create(NULL, kInitialParentWindowRect));
  HWND hwnd = static_cast<HWND>(window);
  MockInfobarContent* content = new MockInfobarContent();
  scoped_ptr<MockHost> host(new MockHost(&infobar_window,
                                         natural_dimensions,
                                         hwnd));

  infobar_window.SetHost(host.get());

  // Used to ensure that GetDesiredSize is only called on an installed
  // InfobarContent.
  testing::Expectation installed;

  // Will hold the frame given to us in InfobarContent::InstallInFrame.
  InfobarContent::Frame* frame = NULL;

  // We could get any number of calls to UpdateLayout. Make sure that, each
  // time, the space reserved by the InfobarWindow equals the space offered to
  // the InfobarContent.
  EXPECT_CALL(*host, CheckReservedSpace(RectAddedToRectXMakesRectY(
      &current_infobar_dimensions, &natural_dimensions)))
      .Times(testing::AnyNumber())
      .WillRepeatedly(ResetFlag(&pending_reserve_space));

  testing::MockFunction<void(std::string check_point_name)> check;

  {
    testing::InSequence s;
    // During Show(), we get an InstallInFrame
    installed = EXPECT_CALL(*content, InstallInFrame(FrameHwndIs(hwnd)))
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&frame),
                                 testing::Return(true)));

    // Allow a call to SetDimensions before InstallInFrame returns.
    EXPECT_CALL(*content, SetDimensions(testing::AllOf(
        RectIsTopXToYOfRectZ(0, infobar_height / 2 - 1, &natural_dimensions),
        RectHeightIsGTEToRectXHeight(&current_infobar_dimensions))))
        .Times(testing::AnyNumber()).WillRepeatedly(testing::DoAll(
            testing::SaveArg<0>(&current_infobar_dimensions),
            CheckSetFlag(&pending_reserve_space)));

    EXPECT_CALL(check, Call("returned from Show"));

    EXPECT_CALL(*content, SetDimensions(testing::AllOf(
        RectIsTopXToYOfRectZ(0, infobar_height / 2 - 1, &natural_dimensions),
        RectHeightIsGTEToRectXHeight(&current_infobar_dimensions))))
        .Times(testing::AtLeast(1)).WillRepeatedly(testing::DoAll(
            testing::SaveArg<0>(&current_infobar_dimensions),
            CheckSetFlag(&pending_reserve_space)));
    EXPECT_CALL(*content, SetDimensions(testing::AllOf(
        RectIsTopXToYOfRectZ(infobar_height / 2,
                             infobar_height - 1,
                             &natural_dimensions),
        RectHeightIsGTEToRectXHeight(&current_infobar_dimensions))))
        .Times(testing::AtLeast(1)).WillRepeatedly(testing::DoAll(
            testing::SaveArg<0>(&current_infobar_dimensions),
            CheckSetFlag(&pending_reserve_space)));
    EXPECT_CALL(*content, SetDimensions(
        RectIsTopXToYOfRectZ(infobar_height,
                             infobar_height,
                             &natural_dimensions)))
        .WillOnce(testing::DoAll(
            testing::SaveArg<0>(&current_infobar_dimensions),
            CheckSetFlag(&pending_reserve_space),
            AsynchronousCloseOnFrame(&message_loop, &frame)));

    EXPECT_CALL(*content, SetDimensions(testing::AllOf(
        RectIsTopXToYOfRectZ(infobar_height / 2 + 1,
                             infobar_height,
                             &natural_dimensions),
        RectHeightIsLTEToRectXHeight(&current_infobar_dimensions))))
        .Times(testing::AtLeast(1)).WillRepeatedly(testing::DoAll(
            testing::SaveArg<0>(&current_infobar_dimensions),
            CheckSetFlag(&pending_reserve_space)));
    EXPECT_CALL(*content, SetDimensions(testing::AllOf(
        RectIsTopXToYOfRectZ(0, infobar_height / 2, &natural_dimensions),
        RectHeightIsLTEToRectXHeight(&current_infobar_dimensions))))
        .Times(testing::AtLeast(1)).WillRepeatedly(testing::DoAll(
            testing::SaveArg<0>(&current_infobar_dimensions),
            CheckSetFlag(&pending_reserve_space)));

    EXPECT_CALL(*content, Die()).WillOnce(QUIT_LOOP(message_loop));
  }

  EXPECT_CALL(*content, GetDesiredSize(80, 0))
      .Times(testing::AnyNumber()).After(installed)
      .WillRepeatedly(testing::Return(infobar_height));

  ASSERT_NO_FATAL_FAILURE(infobar_window.Show(content));

  ASSERT_NO_FATAL_FAILURE(check.Call("returned from Show"));

  ASSERT_NO_FATAL_FAILURE(message_loop.RunFor(10));  // seconds

  window.DestroyWindow();

  ASSERT_FALSE(message_loop.WasTimedOut());
}

TEST(InfobarsInfobarManagerTest, BasicTest) {
  chrome_frame_test::TimedMsgLoop message_loop;

  int infobar_height = 40;
  RECT natural_dimensions = {10, 20, 90, 100 + infobar_height};

  testing::NiceMock<MockTopLevelWindow> window;
  ASSERT_TRUE(window.Create(NULL, kInitialParentWindowRect) != NULL);
  testing::NiceMock<MockChildWindow> child_window;
  ASSERT_TRUE(child_window.Create(window, kInitialChildWindowRect) != NULL);

  HWND parent_hwnd = static_cast<HWND>(window);
  HWND child_hwnd = static_cast<HWND>(child_window);

  MockInfobarContent* content = new MockInfobarContent();
  InfobarContent::Frame* frame = NULL;

  InfobarManager* manager = InfobarManager::Get(parent_hwnd);
  ASSERT_FALSE(manager == NULL);

  EXPECT_CALL(*content, GetDesiredSize(80, 0))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(infobar_height));

  EXPECT_CALL(child_window, OnNcCalcSize(true, testing::_))
      .Times(testing::AnyNumber()).WillRepeatedly(
          RespondToNcCalcSize(0, &natural_dimensions));

  EXPECT_CALL(*content, InstallInFrame(FrameHwndIs(parent_hwnd)))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&frame),
                               testing::Return(true)));

  EXPECT_CALL(*content, SetDimensions(testing::Not(
      RectIsTopXToYOfRectZ(infobar_height,
                           infobar_height,
                           &natural_dimensions)))).Times(testing::AnyNumber());

  EXPECT_CALL(*content, SetDimensions(
      RectIsTopXToYOfRectZ(infobar_height,
                           infobar_height,
                           &natural_dimensions)))
      .Times(testing::AnyNumber())
      .WillOnce(AsynchronousHideOnManager(&message_loop, manager))
      .WillRepeatedly(testing::Return());

  EXPECT_CALL(*content, Die()).WillOnce(QUIT_LOOP(message_loop));

  ASSERT_TRUE(manager->Show(content, TOP_INFOBAR));

  message_loop.RunFor(10);  // seconds

  window.DestroyWindow();

  ASSERT_FALSE(message_loop.WasTimedOut());
}

// TODO(erikwright): Write test for variations on return from default
// OnNcCalcValidRects
