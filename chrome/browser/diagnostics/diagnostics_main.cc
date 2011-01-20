// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_main.h"

#if defined(OS_POSIX)
#include <stdio.h>
#include <unistd.h>
#endif

#include <iostream>

#include "app/app_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/diagnostics/diagnostics_model.h"
#include "chrome/common/chrome_paths.h"
#include "ui/base/ui_base_paths.h"

namespace {
// This is a minimalistic interface to wrap the platform console.  This will be
// eventually replaced by a view that can be subclassed for each platform and
// that the approved look and feel.
class SimpleConsole {
 public:
  enum Color {
    DEFAULT,
    RED,
    GREEN,
  };

  virtual ~SimpleConsole() { }

  // Init must be called before using any other method. If it returns
  // false there would be no console output.
  virtual bool Init() = 0;

  // Writes a string to the console with the current color.
  virtual bool Write(const std::wstring& text) = 0;

  // Reads a string from the console. Internally it may be limited to 256
  // characters.
  virtual bool Read(std::wstring* txt) = 0;

  // Sets the foreground and background color.
  virtual bool SetColor(Color color) = 0;

  // Create an appropriate SimpleConsole instance.  May return NULL if there is
  // no implementation for the current platform.
  static SimpleConsole* Create();
};

#if defined(OS_WIN)
// Wrapper for the windows console operating in high-level IO mode.
class WinConsole : public SimpleConsole {
 public:
  // The ctor allocates a console always. This avoids having to ask
  // the user to start chrome from a command prompt.
  WinConsole()
      : std_out_(INVALID_HANDLE_VALUE),
        std_in_(INVALID_HANDLE_VALUE)  {
  }

  virtual ~WinConsole() {
    ::FreeConsole();
  }

  virtual bool Init() {
    ::AllocConsole();
    return SetIOHandles();
  }

  virtual bool Write(const std::wstring& txt) {
    DWORD sz = txt.size();
    return (TRUE == ::WriteConsoleW(std_out_, txt.c_str(), sz, &sz, NULL));
  }

  // Reads a string from the console. Internally it is limited to 256
  // characters.
  virtual bool Read(std::wstring* txt) {
    wchar_t buf[256];
    DWORD read = sizeof(buf) - sizeof(buf[0]);
    if (!::ReadConsoleW(std_in_, buf, read, &read, NULL))
      return false;
    // Note that |read| is in bytes.
    txt->assign(buf, read/2);
    return true;
  }

  // Sets the foreground and background color.
  virtual bool SetColor(Color color) {
    uint16 color_combo =
        FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
    switch (color) {
      case RED:
        color_combo = FOREGROUND_RED|FOREGROUND_INTENSITY;
        break;
      case GREEN:
        color_combo = FOREGROUND_GREEN|FOREGROUND_INTENSITY;
        break;
      case DEFAULT:
        break;
      default:
        NOTREACHED();
    }
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

  DISALLOW_COPY_AND_ASSIGN(WinConsole);
};

SimpleConsole* SimpleConsole::Create() {
  return new WinConsole();
}

#elif defined(OS_POSIX)

class PosixConsole : public SimpleConsole {
 public:
  PosixConsole() : use_color_(false) { }

  virtual bool Init() {
    // Technically, we should also check the terminal capabilities before using
    // color, but in practice this is unlikely to be an issue.
    use_color_ = isatty(STDOUT_FILENO);
    return true;
  }

  virtual bool Write(const std::wstring& text) {
    printf("%s", base::SysWideToNativeMB(text).c_str());
    return true;
  }

  virtual bool Read(std::wstring* txt) {
    std::string input;
    if (!std::getline(std::cin, input)) {
      std::cin.clear();
      return false;
    }
    *txt = UTF8ToWide(input);
    return true;
  }

  virtual bool SetColor(Color color) {
    if (!use_color_)
      return false;

    const char* code = "\033[m";
    switch (color) {
      case RED:
        code = "\033[1;31m";
        break;
      case GREEN:
        code = "\033[1;32m";
        break;
      case DEFAULT:
        break;
      default:
        NOTREACHED();
    }
    printf("%s", code);
    return true;
  }

 private:
  bool use_color_;

  DISALLOW_COPY_AND_ASSIGN(PosixConsole);
};

SimpleConsole* SimpleConsole::Create() {
  return new PosixConsole();
}

#else  // !defined(OS_WIN) && !defined(OS_POSIX)

SimpleConsole* SimpleConsole::Create() {
  return NULL;
}
#endif

// This class wraps a SimpleConsole for the specific use case of
// writing the results of the diagnostic tests.
// TODO(cpu) figure out the localization strategy.
class TestWriter {
 public:
  // The |console| must be valid and properly initialized. This
  // class does not own it.
  explicit TestWriter(SimpleConsole* console)
      : console_(console),
        failures_(0) {
  }

  // How many tests reported failure.
  int failures() { return failures_; }

  // Write an informational line of text in white over black.
  bool WriteInfoText(const std::wstring& txt) {
    console_->SetColor(SimpleConsole::DEFAULT);
    return console_->Write(txt);
  }

  // Write a result block. It consist of two lines. The first line
  // has [PASS] or [FAIL] with |name| and the second line has
  // the text in |extra|.
  bool WriteResult(bool success, const std::wstring& name,
                   const std::wstring& extra) {
    if (success) {
      console_->SetColor(SimpleConsole::GREEN);
      console_->Write(L"[PASS] ");
    } else {
      console_->SetColor(SimpleConsole::RED);
      console_->Write(L"[FAIL] ");
      failures_++;
    }
    WriteInfoText(name + L"\n");
    std::wstring second_line(L"   ");
    second_line.append(extra);
    return WriteInfoText(second_line + L"\n\n");
  }

 private:

  SimpleConsole* console_;

  // Keeps track of how many tests reported failure.
  int failures_;

  DISALLOW_COPY_AND_ASSIGN(TestWriter);
};

std::wstring PrintableUSCurrentTime() {
  base::Time::Exploded exploded = {0};
  base::Time::Now().UTCExplode(&exploded);
  return StringPrintf(L"%d:%d:%d.%d:%d:%d",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second);
}

// This class is a basic test controller. In this design the view (TestWriter)
// and the model (DiagnosticsModel) do not talk to each other directly but they
// are mediated by the controller. This has a name: 'passive view'.
// More info at http://martinfowler.com/eaaDev/PassiveScreen.html
class TestController : public DiagnosticsModel::Observer {
 public:
  explicit TestController(TestWriter* writer)
      : model_(NULL),
        writer_(writer) {
  }

  // Run all the diagnostics of |model| and invoke the view as the model
  // callbacks arrive.
  void Run(DiagnosticsModel* model) {
    std::wstring title(L"Chrome Diagnostics Mode (");
    writer_->WriteInfoText(title.append(PrintableUSCurrentTime()) + L")\n");
    if (!model) {
      writer_->WriteResult(false, L"Diagnostics start", L"model is null");
      return;
    }
    bool icu_result = icu_util::Initialize();
    if (!icu_result) {
      writer_->WriteResult(false, L"Diagnostics start", L"ICU failure");
      return;
    }
    int count = model->GetTestAvailableCount();
    writer_->WriteInfoText(StringPrintf(L"%d available test(s)\n\n", count));
    model->RunAll(this);
  }

  // Next four are overridden from DiagnosticsModel::Observer.
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
    if (writer_->failures() > 0) {
      writer_->WriteInfoText(StringPrintf(L"DONE. %d failure(s)\n\n",
                             writer_->failures()));
    } else {
      writer_->WriteInfoText(L"DONE\n\n");
    }
  }

 private:
  void ShowResult(DiagnosticsModel::TestInfo& test_info) {
    bool success = (DiagnosticsModel::TEST_OK == test_info.GetResult());
    writer_->WriteResult(success, UTF16ToWide(test_info.GetTitle()),
                         UTF16ToWide(test_info.GetAdditionalInfo()));
  }

  DiagnosticsModel* model_;
  TestWriter* writer_;

  DISALLOW_COPY_AND_ASSIGN(TestController);
};
}  // namespace

// This entry point is called from ChromeMain() when very few things
// have been initialized. To wit:
// -(win)   Breakpad
// -(macOS) base::EnableTerminationOnHeapCorruption()
// -(macOS) base::EnableTerminationOnOutOfMemory()
// -(all)   RegisterInvalidParamHandler()
// -(all)   base::AtExitManager::AtExitManager()
// -(macOS) base::ScopedNSAutoreleasePool
// -(posix) base::GlobalDescriptors::GetInstance()->Set(kPrimaryIPCChannel)
// -(linux) base::GlobalDescriptors::GetInstance()->Set(kCrashDumpSignal)
// -(posix) setlocale(LC_ALL,..)
// -(all)   CommandLine::Init();

int DiagnosticsMain(const CommandLine& command_line) {
  // If we can't initialize the console exit right away.
  SimpleConsole* console = SimpleConsole::Create();
  if (!console || !console->Init())
    return 1;

  // We need to have the path providers registered. They both
  // return void so there is no early error signal that we can use.
  app::RegisterPathProvider();
  ui::RegisterPathProvider();
  chrome::RegisterPathProvider();

  TestWriter writer(console);
  DiagnosticsModel* model = MakeDiagnosticsModel(command_line);
  TestController controller(&writer);

  // Run all the diagnostic tests.
  controller.Run(model);
  delete model;

  // The "press enter to continue" prompt isn't very unixy, so only do that on
  // Windows.
#if defined(OS_WIN)
  // Block here so the user can see the results.
  writer.WriteInfoText(L"Press [enter] to continue\n");
  std::wstring txt;
  console->Read(&txt);
#endif
  delete console;
  return 0;
}
