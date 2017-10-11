// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/always_on_top_window_killer_win.h"

#include <Windows.h>

#include <ios>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

constexpr char kDialogFoundBeforeTest[] =
    "There is an always on top dialog on the desktop. This was most likely "
    "caused by a previous test and may cause this test to fail. Trying to "
    "close it.";

constexpr char kDialogFoundPostTest[] =
    "There is an always on top dialog on the desktop after running this test. "
    "This was most likely caused by this test and may cause future tests to "
    "fail, trying to close it.";

constexpr char kWindowFoundBeforeTest[] =
    "There is an always on top window on the desktop before running the test. "
    "This may have been caused by a previous test and may cause this test to "
    "fail, class-name=";

constexpr char kWindowFoundPostTest[] =
    "There is an always on top window on the desktop after running the test. "
    "This may have been caused by this test or a previous test and may cause "
    "flake, class-name=";

// A command line switch to specify the output directory into which snapshots
// are to be saved in case an always-on-top window is found.
constexpr char kSnapshotOutputDir[] = "snapshot-output-dir";

// A worker that captures a single frame from a webrtc::DesktopCapturer and then
// runs a callback when done.
class CaptureWorker : public webrtc::DesktopCapturer::Callback {
 public:
  CaptureWorker(std::unique_ptr<webrtc::DesktopCapturer> capturer,
                base::Closure on_done)
      : capturer_(std::move(capturer)), on_done_(std::move(on_done)) {
    capturer_->Start(this);
    capturer_->CaptureFrame();
  }

  // Returns the frame that was captured or null in case of failure.
  std::unique_ptr<webrtc::DesktopFrame> TakeFrame() {
    return std::move(frame_);
  }

 private:
  // webrtc::DesktopCapturer::Callback:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    if (result == webrtc::DesktopCapturer::Result::SUCCESS)
      frame_ = std::move(frame);
    on_done_.Run();
  }

  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  base::Closure on_done_;
  std::unique_ptr<webrtc::DesktopFrame> frame_;
  DISALLOW_COPY_AND_ASSIGN(CaptureWorker);
};

// Captures and returns a snapshot of the screen, or an empty bitmap in case of
// error.
SkBitmap CaptureScreen() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  options.set_disable_effects(false);
  options.set_allow_directx_capturer(true);
  options.set_allow_use_magnification_api(false);
  std::unique_ptr<webrtc::DesktopCapturer> capturer(
      webrtc::DesktopCapturer::CreateScreenCapturer(options));

  // Grab a single frame.
  std::unique_ptr<webrtc::DesktopFrame> frame;
  {
    // While webrtc::DesktopCapturer seems to be synchronous, comments in its
    // implementation seem to indicate that it may require a UI message loop on
    // its thread.
    base::MessageLoopForUI message_loop;
    base::RunLoop run_loop;
    CaptureWorker worker(std::move(capturer), run_loop.QuitClosure());
    run_loop.Run();
    frame = worker.TakeFrame();
  }

  if (!frame)
    return SkBitmap();

  // Create an image from the frame.
  SkBitmap result;
  result.allocN32Pixels(frame->size().width(), frame->size().height(), true);
  memcpy(result.getAddr32(0, 0), frame->data(),
         frame->size().width() * frame->size().height() *
             webrtc::DesktopFrame::kBytesPerPixel);
  return result;
}

// Saves a snapshot of the screen to a file in |output_dir|, returning the path
// to the file if created. An empty path is returned if no new snapshot is
// created.
base::FilePath SaveSnapshot(const base::FilePath& output_dir) {
  // Create the output file.
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  base::FilePath output_path(
      output_dir.Append(base::FilePath(base::StringPrintf(
          L"ss_%4d%02d%02d%02d%02d%02d_%03d.png", exploded.year, exploded.month,
          exploded.day_of_month, exploded.hour, exploded.minute,
          exploded.second, exploded.millisecond))));
  base::File file(output_path, base::File::FLAG_CREATE |
                                   base::File::FLAG_WRITE |
                                   base::File::FLAG_SHARE_DELETE |
                                   base::File::FLAG_CAN_DELETE_ON_CLOSE);
  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_EXISTS) {
      LOG(INFO) << "Skipping screen snapshot since it is already present: "
                << output_path.BaseName();
    } else {
      LOG(ERROR) << "Failed to create snapshot output file \"" << output_path
                 << "\" with error " << file.error_details();
    }
    return base::FilePath();
  }

  // Delete the output file in case of any error.
  file.DeleteOnClose(true);

  // Take the snapshot and encode it.
  SkBitmap screen = CaptureScreen();
  if (screen.drawsNothing()) {
    LOG(ERROR) << "Failed to capture a frame of the screen for a snapshot.";
    return base::FilePath();
  }

  std::vector<unsigned char> encoded;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(CaptureScreen(), false, &encoded)) {
    LOG(ERROR) << "Failed to PNG encode screen snapshot.";
    return base::FilePath();
  }

  // Write it to disk.
  const int to_write = base::checked_cast<int>(encoded.size());
  int written =
      file.WriteAtCurrentPos(reinterpret_cast<char*>(encoded.data()), to_write);
  if (written != to_write) {
    LOG(ERROR) << "Failed to write entire snapshot to file";
    return base::FilePath();
  }

  // Keep the output file.
  file.DeleteOnClose(false);
  return output_path;
}

// A window enumerator that searches for always-on-top windows. A snapshot of
// the screen is saved if any unexpected on-top windows are found.
class WindowEnumerator {
 public:
  explicit WindowEnumerator(RunType run_type);
  void Run();

 private:
  // An EnumWindowsProc invoked by EnumWindows once for each window.
  static BOOL CALLBACK OnWindowProc(HWND hwnd, LPARAM l_param);

  // Returns true if |hwnd| is an always-on-top window.
  static bool IsTopmostWindow(HWND hwnd);

  // Returns the class name of |hwnd| or an empty string in case of error.
  static base::string16 GetWindowClass(HWND hwnd);

  // Returns true if |class_name| is the name of a system dialog.
  static bool IsSystemDialogClass(const base::string16& class_name);

  // Returns true if |class_name| is the name of a window owned by the Windows
  // shell.
  static bool IsShellWindowClass(const base::string16& class_name);

  // Main processing function run for each window.
  BOOL OnWindow(HWND hwnd);

  const base::FilePath output_dir_;
  const RunType run_type_;
  bool saved_snapshot_;
  DISALLOW_COPY_AND_ASSIGN(WindowEnumerator);
};

WindowEnumerator::WindowEnumerator(RunType run_type)
    : output_dir_(base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kSnapshotOutputDir)),
      run_type_(run_type),
      saved_snapshot_(false) {}

void WindowEnumerator::Run() {
  ::EnumWindows(&OnWindowProc, reinterpret_cast<LPARAM>(this));
}

// static
BOOL CALLBACK WindowEnumerator::OnWindowProc(HWND hwnd, LPARAM l_param) {
  return reinterpret_cast<WindowEnumerator*>(l_param)->OnWindow(hwnd);
}

// static
bool WindowEnumerator::IsTopmostWindow(HWND hwnd) {
  const LONG ex_styles = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  return (ex_styles & WS_EX_TOPMOST) != 0;
}

// static
base::string16 WindowEnumerator::GetWindowClass(HWND hwnd) {
  wchar_t buffer[257];  // Max is 256.
  buffer[arraysize(buffer) - 1] = L'\0';
  int name_len = ::GetClassName(hwnd, &buffer[0], arraysize(buffer));
  if (name_len <= 0 || static_cast<size_t>(name_len) >= arraysize(buffer))
    return base::string16();
  return base::string16(&buffer[0], name_len);
}

// static
bool WindowEnumerator::IsSystemDialogClass(const base::string16& class_name) {
  return class_name == L"#32770";
}

// static
bool WindowEnumerator::IsShellWindowClass(const base::string16& class_name) {
  // 'Button' is the start button, 'Shell_TrayWnd' the taskbar, and
  // 'Shell_SecondaryTrayWnd' is the taskbar on non-primary displays.
  return class_name == L"Button" || class_name == L"Shell_TrayWnd" ||
         class_name == L"Shell_SecondaryTrayWnd";
}

BOOL WindowEnumerator::OnWindow(HWND hwnd) {
  const BOOL kContinueIterating = TRUE;

  if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd) || !IsTopmostWindow(hwnd))
    return kContinueIterating;

  base::string16 class_name = GetWindowClass(hwnd);
  if (class_name.empty())
    return kContinueIterating;

  // Ignore specific windows owned by the shell.
  if (IsShellWindowClass(class_name))
    return kContinueIterating;

  // Something unexpected was found. Save a snapshot of the screen if one hasn't
  // already been saved and an output directory was specified.
  if (!saved_snapshot_ && !output_dir_.empty()) {
    base::FilePath snapshot_file = SaveSnapshot(output_dir_);
    if (!snapshot_file.empty()) {
      saved_snapshot_ = true;
      LOG(ERROR) << "Wrote snapshot to file " << snapshot_file;
    }
  }

  // System dialogs may be present if a child process triggers an assert(), for
  // example.
  if (IsSystemDialogClass(class_name)) {
    LOG(ERROR) << (run_type_ == RunType::BEFORE_TEST ? kDialogFoundBeforeTest
                                                     : kDialogFoundPostTest);
    // We don't own the dialog, so we can't destroy it. CloseWindow()
    // results in iconifying the window. An alternative may be to focus it,
    // then send return and wait for close. As we reboot machines running
    // interactive ui tests at least every 12 hours we're going with the
    // simple for now.
    CloseWindow(hwnd);
    return kContinueIterating;
  }

  // All other always-on-top windows may be problematic, but in theory tests
  // should not be creating an always on top window that outlives the test. Log
  // attributes of the window in case there are problems.
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(hwnd, &process_id);

  base::Process process = base::Process::Open(process_id);
  base::string16 process_path(MAX_PATH, L'\0');
  if (process.IsValid()) {
    // It's possible that the actual process owning |hwnd| has gone away and
    // that a new process using the same PID has appeared. If this turns out to
    // be an issue, we could fetch the process start time here and compare it
    // with the time just before getting |thread_id| above. This is likely
    // overkill for diagnostic purposes.
    DWORD str_len = process_path.size();
    if (!::QueryFullProcessImageName(process.Handle(), 0, &process_path[0],
                                     &str_len) ||
        str_len >= MAX_PATH) {
      str_len = 0;
    }
    process_path.resize(str_len);
  }
  LOG(ERROR) << (run_type_ == RunType::BEFORE_TEST ? kWindowFoundBeforeTest
                                                   : kWindowFoundPostTest)
             << class_name << " process_id=" << process_id
             << " thread_id=" << thread_id << " process_path=" << process_path;

  return kContinueIterating;
}

}  // namespace

void KillAlwaysOnTopWindows(RunType run_type) {
  WindowEnumerator(run_type).Run();
}
