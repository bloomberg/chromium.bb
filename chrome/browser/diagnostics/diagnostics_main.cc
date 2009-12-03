// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_main.h"

#include "app/app_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/diagnostics/diagnostics_model.h"
#include "chrome/common/chrome_paths.h"

#if defined(OS_WIN)
#include "base/win_util.h"

namespace {
// This is a minimalistic class to wrap the windows console operating
// in high-level IO mode. This will be eventually replaced by a view
// that can be subclassed for each platform and that the approved look
// and feel.
class SimpleConsole {
 public:
  // The ctor allocates a console always. This avoids having to ask
  // the user to start chrome from a command prompt.
  SimpleConsole()
      : std_out_(INVALID_HANDLE_VALUE),
        std_in_(INVALID_HANDLE_VALUE)  {
  }

  ~SimpleConsole() {
    ::FreeConsole();
  }
  // Init must be called before using any other method. If it returns
  // false there would be no console output.
  bool Init() {
    ::AllocConsole();
    return SetIOHandles();
  }

  // Writes a string to the console with the current color.
  bool Write(const std::wstring& txt) {
    DWORD sz = txt.size();
    return (TRUE == ::WriteConsoleW(std_out_, txt.c_str(), sz, &sz, NULL));
  }

  // Reads a string from the console. Internally it is limited to 256
  // characters.
  bool Read(std::wstring* txt) {
    wchar_t buf[256];
    DWORD read = sizeof(buf) - sizeof(buf[0]);
    if (!::ReadConsoleW(std_in_, buf, read, &read, NULL))
      return false;
    // Note that |read| is in bytes.
    txt->assign(buf, read/2);
    return true;
  }

  // Sets the foreground and backgroudn color. See |kRedFG| or |kGreenFG|
  // below for examples how to combine colors.
  bool SetColor(uint16 color_combo) {
    return (TRUE == ::SetConsoleTextAttribute(std_out_, color_combo));
  }

 private:
  bool SetIOHandles() {
    std_out_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
    std_in_ = ::GetStdHandle(STD_INPUT_HANDLE);
    return ((std_out_ != INVALID_HANDLE_VALUE) &&
            (std_in_ != INVALID_HANDLE_VALUE));
  }

  // The input and output handles to the screen. They seem to be
  // implemented as pipes but they have non-documented protocol.
  HANDLE std_out_;
  HANDLE std_in_;

  DISALLOW_COPY_AND_ASSIGN(SimpleConsole);
};

// This class wraps a SimpleConsole for the specific use case of
// writing the results of the diagnostic tests.
// TODO(cpu) figure out the localization strategy.
class TestWriter {
 public:
  // The |console| must be valid and properly initialized. This
  // class does not own it.
  explicit TestWriter(SimpleConsole* console) : console_(console) {
  }

  // Write an informational line of text in white over black.
  bool WriteInfoText(const std::wstring& txt) {
    console_->SetColor(kWhiteFG);
    return console_->Write(txt);
  }

  // Write a result block. It consist of two lines. The first line
  // has [PASS] or [FAIL] with |name| and the second line has
  // the text in |extra|.
  bool WriteResult(bool success, const std::wstring& name,
                   const std::wstring& extra) {
    if (success) {
      console_->SetColor(kGreenFG);
      console_->Write(L"[PASS] ");
    } else {
      console_->SetColor(kRedFG);
      console_->Write(L"[FAIL] ");
    }
    WriteInfoText(name + L"\n");
    std::wstring second_line(L"   ");
    second_line.append(extra);
    return WriteInfoText(second_line + L"\n\n");
  }

 private:
  // constants for the colors we use.
  static const uint16 kRedFG = FOREGROUND_RED|FOREGROUND_INTENSITY;
  static const uint16 kGreenFG = FOREGROUND_GREEN|FOREGROUND_INTENSITY;
  static const uint16 kWhiteFG = kRedFG | kGreenFG | FOREGROUND_BLUE;

  SimpleConsole* console_;

  DISALLOW_COPY_AND_ASSIGN(TestWriter);
};

// This class is a basic test controller. In this design the view (TestWriter)
// and the model (DiagnosticsModel) do not talk to each other directly but they
// are mediated by the controller. This has a name: 'passive view'.
// More info at http://martinfowler.com/eaaDev/PassiveScreen.html
class TestController : public DiagnosticsModel::Observer {
 public:
  explicit TestController(TestWriter* writer) : writer_(writer) {
  }

  // Run all the diagnostics of |model| and invoke the view as the model
  // callbacks arrive.
  void Run(DiagnosticsModel* model) {
    writer_->WriteInfoText(L"Chrome Diagnostics\n");
    if (!model) {
      writer_->WriteResult(false, L"Diagnostics start", L"model is null");
      return;
    }
    int count = model->GetTestAvailableCount();
    writer_->WriteInfoText(StringPrintf(L"%d available test(s)\n\n", count));
    model->RunAll(this);
  }

  // Next four are overriden from DiagnosticsModel::Observer
  virtual void OnProgress(int id, int percent, DiagnosticsModel* model) {
  }

  virtual void OnSkipped(int id, DiagnosticsModel* model) {
    // TODO(cpu): display skipped tests.
  }

  virtual void OnFinished(int id, DiagnosticsModel* model) {
    // As each test completes we output the results.
    ShowResult(model->GetTest(id));
  }

  virtual void OnDoneAll(DiagnosticsModel* model) {
    writer_->WriteInfoText(L"DONE\n\n");
  }

 private:
  void ShowResult(DiagnosticsModel::TestInfo& test_info) {
    bool success = (DiagnosticsModel::TEST_OK == test_info.GetResult());
    writer_->WriteResult(success, test_info.GetTitle(),
                         test_info.GetAdditionalInfo());
  }

  DiagnosticsModel* model_;
  TestWriter* writer_;

  DISALLOW_COPY_AND_ASSIGN(TestController);
};

}

// This entry point is called from ChromeMain() when very few things
// have been initialized. To wit:
// -(win)   Breakpad
// -(macOS) base::EnableTerminationOnHeapCorruption()
// -(macOS) base::EnableTerminationOnOutOfMemory()
// -(all)   RegisterInvalidParamHandler()
// -(all)   base::AtExitManager::AtExitManager()
// -(macOS) base::ScopedNSAutoreleasePool
// -(posix) Singleton<base::GlobalDescriptors>::Set(kPrimaryIPCChannel)
// -(linux) Singleton<base::GlobalDescriptors>::Set(kCrashDumpSignal)
// -(posix) setlocale(LC_ALL,..)
// -(all)   CommandLine::Init();

int DiagnosticsMain(const CommandLine& command_line) {
  // If we can't initialize the console exit right away.
  SimpleConsole console;
  if (!console.Init())
    return 1;

  // We need to have the path providers registered. They both
  // return void so there is no early error signal that we can use.
  app::RegisterPathProvider();
  chrome::RegisterPathProvider();

  TestWriter writer(&console);
  DiagnosticsModel* model = MakeDiagnosticsModel();
  TestController controller(&writer);

  // Run all the diagnostic tests.
  controller.Run(model);
  delete model;

  // Block here so the user can see the results.
  writer.WriteInfoText(L"Press [enter] to continue\n");
  std::wstring txt;
  console.Read(&txt);
  return 0;
}

#else  // defined(OS_WIN)

int DiagnosticsMain(const CommandLine& command_line) {
  return 0;
}

#endif
