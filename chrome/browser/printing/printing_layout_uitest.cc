// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/threading/simple_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"
#include "printing/image.h"
#include "printing/printing_test.h"

namespace {

using printing::Image;

const char kGenerateSwitch[] = "print-layout-generate";
const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

class PrintingLayoutTest : public PrintingTest<UITest> {
 public:
  PrintingLayoutTest() {
    emf_path_ = browser_directory_.AppendASCII("metafile_dumps");
    launch_arguments_.AppendSwitchPath("debug-print", emf_path_);
    show_window_ = true;
  }

  virtual void SetUp() {
    // Make sure there is no left overs.
    CleanupDumpDirectory();
    UITest::SetUp();
  }

  virtual void TearDown() {
    UITest::TearDown();
    file_util::Delete(emf_path_, true);
  }

 protected:
  void PrintNowTab() {
    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->PrintNow());
  }

  // Finds the dump for the last print job and compares it to the data named
  // |verification_name|. Compares the saved printed job pixels with the test
  // data pixels and returns the percentage of different pixels; 0 for success,
  // [0, 100] for failure.
  double CompareWithResult(const std::wstring& verification_name) {
    FilePath test_result(ScanFiles(verification_name));
    if (test_result.value().empty()) {
      // 100% different, the print job buffer is not there.
      return 100.;
    }

    std::wstring verification_file(test_data_directory_.value());
    file_util::AppendToPath(&verification_file, L"printing");
    file_util::AppendToPath(&verification_file, verification_name);
    FilePath emf(verification_file + L".emf");
    FilePath png(verification_file + L".png");

    // Looks for Cleartype override.
    if (file_util::PathExists(
            FilePath::FromWStringHack(verification_file + L"_cleartype.png")) &&
        IsClearTypeEnabled()) {
      png = FilePath(verification_file + L"_cleartype.png");
    }

    if (GenerateFiles()) {
      // Copy the .emf and generate an .png.
      file_util::CopyFile(test_result, emf);
      Image emf_content(emf);
      emf_content.SaveToPng(png);
      // Saving is always fine.
      return 0;
    } else {
      // File compare between test and result.
      Image emf_content(emf);
      Image test_content(test_result);
      Image png_content(png);
      double diff_emf = emf_content.PercentageDifferent(test_content);

      EXPECT_EQ(0., diff_emf) << verification_name <<
          L" original size:" << emf_content.size() <<
          L" result size:" << test_content.size();
      if (diff_emf) {
        // Backup the result emf file.
        file_util::CopyFile(test_result, FilePath(
              verification_file + L"_failed.emf"));
      }

      // This verification is only to know that the EMF rendering stays
      // immutable.
      double diff_png = emf_content.PercentageDifferent(png_content);
      EXPECT_EQ(0., diff_png) << verification_name <<
          L" original size:" << emf_content.size() <<
          L" result size:" << test_content.size();
      if (diff_png) {
        // Backup the rendered emf file to detect the rendering difference.
        emf_content.SaveToPng(FilePath(verification_file + L"_rendering.png"));
      }
      return std::max(diff_png, diff_emf);
    }
  }

  // Makes sure the directory exists and is empty.
  void CleanupDumpDirectory() {
    EXPECT_TRUE(file_util::DieFileDie(emf_path(), true));
    EXPECT_TRUE(file_util::CreateDirectory(emf_path()));
  }

  // Returns if Clear Type is currently enabled.
  static bool IsClearTypeEnabled() {
    BOOL ct_enabled = 0;
    if (SystemParametersInfo(SPI_GETCLEARTYPE, 0, &ct_enabled, 0) && ct_enabled)
      return true;
    UINT smoothing = 0;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothing, 0) &&
        smoothing == FE_FONTSMOOTHINGCLEARTYPE)
      return true;
    return false;
  }

 private:
  // Verifies that there is one .emf and one .prn file in the dump directory.
  // Returns the path of the .emf file and deletes the .prn file.
  std::wstring ScanFiles(const std::wstring& verification_name) {
    // Try to 10 seconds.
    std::wstring emf_file;
    std::wstring prn_file;
    bool found_emf = false;
    bool found_prn = false;
    for (int i = 0; i < 100; ++i) {
      file_util::FileEnumerator enumerator(emf_path(), false,
          file_util::FileEnumerator::FILES);
      emf_file.clear();
      prn_file.clear();
      found_emf = false;
      found_prn = false;
      FilePath file;
      while (!(file = enumerator.Next()).empty()) {
        std::wstring ext = file.Extension();
        if (base::strcasecmp(WideToUTF8(ext).c_str(), ".emf") == 0) {
          EXPECT_FALSE(found_emf) << "Found a leftover .EMF file: \"" <<
              emf_file << "\" and \"" << file.value() <<
              "\" when looking for \"" << verification_name << "\"";
          found_emf = true;
          emf_file = file.value();
          continue;
        }
        if (base::strcasecmp(WideToUTF8(ext).c_str(), ".prn") == 0) {
          EXPECT_FALSE(found_prn) << "Found a leftover .PRN file: \"" <<
              prn_file << "\" and \"" << file.value() <<
              "\" when looking for \"" << verification_name << "\"";
          prn_file = file.value();
          found_prn = true;
          file_util::Delete(file, false);
          continue;
        }
        EXPECT_TRUE(false);
      }
      if (found_emf && found_prn)
        break;
      base::PlatformThread::Sleep(100);
    }
    EXPECT_TRUE(found_emf) << ".PRN file is: " << prn_file;
    EXPECT_TRUE(found_prn) << ".EMF file is: " << emf_file;
    return emf_file;
  }

  static bool GenerateFiles() {
    return CommandLine::ForCurrentProcess()->HasSwitch(kGenerateSwitch);
  }

  const FilePath& emf_path() const { return emf_path_; }

  FilePath emf_path_;

  DISALLOW_COPY_AND_ASSIGN(PrintingLayoutTest);
};

// Tests that don't need UI access.
class PrintingLayoutTestHidden : public PrintingLayoutTest {
 public:
  PrintingLayoutTestHidden() {
    show_window_ = false;
  }
};

class PrintingLayoutTextTest : public PrintingLayoutTest {
  typedef PrintingLayoutTest Parent;
 public:
  // Returns if the test is disabled.
  // http://crbug.com/64869 Until the issue is fixed, disable the test if
  // ClearType is enabled.
  static bool IsTestCaseDisabled() {
    return Parent::IsTestCaseDisabled() || IsClearTypeEnabled();
  }
};

// Finds the first dialog window owned by owner_process.
HWND FindDialogWindow(DWORD owner_process) {
  HWND dialog_window(NULL);
  for (;;) {
    dialog_window = FindWindowEx(NULL,
                                 dialog_window,
                                 MAKEINTATOM(32770),
                                 NULL);
    if (!dialog_window)
      break;

    // The dialog must be owned by our target process.
    DWORD process_id = 0;
    GetWindowThreadProcessId(dialog_window, &process_id);
    if (process_id == owner_process)
      break;
  }
  return dialog_window;
}

// Tries to close a dialog window.
bool CloseDialogWindow(HWND dialog_window) {
  LRESULT res = SendMessage(dialog_window, DM_GETDEFID, 0, 0);
  if (!res)
    return false;
  EXPECT_EQ(DC_HASDEFID, HIWORD(res));
  WORD print_button_id = LOWORD(res);
  res = SendMessage(
      dialog_window,
      WM_COMMAND,
      print_button_id,
      reinterpret_cast<LPARAM>(GetDlgItem(dialog_window, print_button_id)));
  return res == 0;
}

// Dismiss the first dialog box owned by owner_process by "executing" the
// default button.
class DismissTheWindow : public base::DelegateSimpleThread::Delegate {
 public:
  explicit DismissTheWindow(DWORD owner_process)
      : owner_process_(owner_process) {
  }

  virtual void Run() {
    HWND dialog_window;
    for (;;) {
      // First enumerate the windows.
      dialog_window = FindDialogWindow(owner_process_);

      // Try to close it.
      if (dialog_window) {
        if (CloseDialogWindow(dialog_window)) {
          break;
        }
      }
      base::PlatformThread::Sleep(10);
    }

    // Now verify that it indeed closed itself.
    while (IsWindow(dialog_window)) {
      CloseDialogWindow(dialog_window);
      base::PlatformThread::Sleep(10);
    }
  }

  DWORD owner_process() { return owner_process_; }

 private:
  DWORD owner_process_;
};

}  // namespace

// Fails, see http://crbug.com/7721.
TEST_F(PrintingLayoutTextTest, DISABLED_Complex) {
  if (IsTestCaseDisabled())
    return;

  DismissTheWindow dismisser(base::GetProcId(process()));
  base::DelegateSimpleThread close_printdlg_thread(&dismisser,
                                                   "close_printdlg_thread");

  // Print a document, check its output.
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  NavigateToURL(test_server.GetURL("files/printing/test1.html"));
  close_printdlg_thread.Start();
  PrintNowTab();
  close_printdlg_thread.Join();
  EXPECT_EQ(0., CompareWithResult(L"test1"));
}

struct TestPool {
  const char* source;
  const wchar_t* result;
};

const TestPool kTestPool[] = {
  // ImagesB&W
  "files/printing/test2.html", L"test2",
  // ImagesTransparent
  "files/printing/test3.html", L"test3",
  // ImageColor
  "files/printing/test4.html", L"test4",
};

// http://crbug.com/7721
TEST_F(PrintingLayoutTestHidden, DISABLED_ManyTimes) {
  if (IsTestCaseDisabled())
    return;

  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  DismissTheWindow dismisser(base::GetProcId(process()));

  ASSERT_GT(arraysize(kTestPool), 0u);
  for (int i = 0; i < arraysize(kTestPool); ++i) {
    if (i)
      CleanupDumpDirectory();
    const TestPool& test = kTestPool[i % arraysize(kTestPool)];
    NavigateToURL(test_server.GetURL(test.source));
    base::DelegateSimpleThread close_printdlg_thread1(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread1.Start();
    PrintNowTab();
    close_printdlg_thread1.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    base::DelegateSimpleThread close_printdlg_thread2(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread2.Start();
    PrintNowTab();
    close_printdlg_thread2.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    base::DelegateSimpleThread close_printdlg_thread3(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread3.Start();
    PrintNowTab();
    close_printdlg_thread3.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    base::DelegateSimpleThread close_printdlg_thread4(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread4.Start();
    PrintNowTab();
    close_printdlg_thread4.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
  }
}

// Prints a popup and immediately closes it. Disabled because it crashes.
TEST_F(PrintingLayoutTest, DISABLED_Delayed) {
  if (IsTestCaseDisabled())
    return;

  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  {
    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    bool is_timeout = true;
    GURL url = test_server.GetURL("files/printing/popup_delayed_print.htm");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));

    DismissTheWindow dismisser(base::GetProcId(process()));
    base::DelegateSimpleThread close_printdlg_thread(&dismisser,
                                                     "close_printdlg_thread");
    close_printdlg_thread.Start();
    close_printdlg_thread.Join();

    // Force a navigation elsewhere to verify that it's fine with it.
    url = test_server.GetURL("files/printing/test1.html");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));
  }
  CloseBrowserAndServer();

  EXPECT_EQ(0., CompareWithResult(L"popup_delayed_print"))
      << L"popup_delayed_print";
}

// Prints a popup and immediately closes it. http://crbug.com/7721
TEST_F(PrintingLayoutTest, DISABLED_IFrame) {
  if (IsTestCaseDisabled())
    return;

  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  {
    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    GURL url = test_server.GetURL("files/printing/iframe.htm");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));

    DismissTheWindow dismisser(base::GetProcId(process()));
    base::DelegateSimpleThread close_printdlg_thread(&dismisser,
                                                     "close_printdlg_thread");
    close_printdlg_thread.Start();
    close_printdlg_thread.Join();

    // Force a navigation elsewhere to verify that it's fine with it.
    url = test_server.GetURL("files/printing/test1.html");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));
  }
  CloseBrowserAndServer();

  EXPECT_EQ(0., CompareWithResult(L"iframe")) << L"iframe";
}
