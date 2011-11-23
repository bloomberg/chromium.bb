// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/process_util.h"

#if !defined(OS_MACOSX)
#include "base/at_exit.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_POSIX)
#include "chrome/common/auto_start_linux.h"
#include <glib.h>
#endif

#if defined(USE_AURA)
// This test fails http://crbug.com/84854, and is very flaky on CrOS and
// somewhat flaky on other Linux.
#define MAYBE_ForceShutdown FAILS_ForceShutdown
#else
#if defined(OS_LINUX)
#define MAYBE_ForceShutdown FLAKY_ForceShutdown
#else
#define MAYBE_ForceShutdown ForceShutdown
#endif
#endif

namespace {

bool g_good_shutdown = false;

void ShutdownTask(MessageLoop* loop) {
  // Quit the main message loop.
  ASSERT_FALSE(g_good_shutdown);
  g_good_shutdown = true;
  loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

}  // namespace

TEST(ServiceProcessUtilTest, ScopedVersionedName) {
  std::string test_str = "test";
  std::string scoped_name = GetServiceProcessScopedVersionedName(test_str);
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());
  EXPECT_TRUE(EndsWith(scoped_name, test_str, true));
  EXPECT_NE(std::string::npos, scoped_name.find(version_info.Version()));
}

class ServiceProcessStateTest : public base::MultiProcessTest {
 public:
  ServiceProcessStateTest();
  ~ServiceProcessStateTest();
  virtual void SetUp();
  base::MessageLoopProxy* IOMessageLoopProxy() {
    return io_thread_.message_loop_proxy();
  }
  void LaunchAndWait(const std::string& name);

 private:
  // This is used to release the ServiceProcessState singleton after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::Thread io_thread_;
};

ServiceProcessStateTest::ServiceProcessStateTest()
    : io_thread_("ServiceProcessStateTestThread") {
}

ServiceProcessStateTest::~ServiceProcessStateTest() {
}

void ServiceProcessStateTest::SetUp() {
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  ASSERT_TRUE(io_thread_.StartWithOptions(options));
}

void ServiceProcessStateTest::LaunchAndWait(const std::string& name) {
  base::ProcessHandle handle = SpawnChild(name, false);
  ASSERT_TRUE(handle);
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(handle, &exit_code));
  ASSERT_EQ(exit_code, 0);
}

TEST_F(ServiceProcessStateTest, Singleton) {
  ServiceProcessState state;
  ASSERT_TRUE(state.Initialize());
  LaunchAndWait("ServiceProcessStateTestSingleton");
}

TEST_F(ServiceProcessStateTest, ReadyState) {
  ASSERT_FALSE(CheckServiceProcessReady());
  ServiceProcessState state;
  ASSERT_TRUE(state.Initialize());
  ASSERT_TRUE(state.SignalReady(IOMessageLoopProxy(), base::Closure()));
  LaunchAndWait("ServiceProcessStateTestReadyTrue");
  state.SignalStopped();
  LaunchAndWait("ServiceProcessStateTestReadyFalse");
}

TEST_F(ServiceProcessStateTest, AutoRun) {
  ServiceProcessState state;
  ASSERT_TRUE(state.AddToAutoRun());
  scoped_ptr<CommandLine> autorun_command_line;
#if defined(OS_WIN)
  std::string value_name = GetServiceProcessScopedName("_service_run");
  string16 value;
  EXPECT_TRUE(base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER,
                                                UTF8ToWide(value_name),
                                                &value));
  autorun_command_line.reset(new CommandLine(CommandLine::FromString(value)));
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#if defined(GOOGLE_CHROME_BUILD)
  std::string base_desktop_name = "google-chrome-service.desktop";
#else  // CHROMIUM_BUILD
  std::string base_desktop_name = "chromium-service.desktop";
#endif
  std::string exec_value;
  EXPECT_TRUE(AutoStart::GetAutostartFileValue(
      GetServiceProcessScopedName(base_desktop_name), "Exec", &exec_value));
  GError *error = NULL;
  gchar **argv = NULL;
  gint argc = 0;
  if (g_shell_parse_argv(exec_value.c_str(), &argc, &argv, &error)) {
    autorun_command_line.reset(new CommandLine(argc, argv));
    g_strfreev(argv);
  } else {
    ADD_FAILURE();
    g_error_free(error);
  }
#endif  // defined(OS_WIN)
  if (autorun_command_line.get()) {
    EXPECT_EQ(autorun_command_line->GetSwitchValueASCII(switches::kProcessType),
              std::string(switches::kServiceProcess));
  }
  ASSERT_TRUE(state.RemoveFromAutoRun());
#if defined(OS_WIN)
  EXPECT_FALSE(base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER,
                                                 UTF8ToWide(value_name),
                                                 &value));
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  EXPECT_FALSE(AutoStart::GetAutostartFileValue(
      GetServiceProcessScopedName(base_desktop_name), "Exec", &exec_value));
#endif  // defined(OS_WIN)
}

TEST_F(ServiceProcessStateTest, SharedMem) {
  std::string version;
  base::ProcessId pid;
#if defined(OS_WIN)
  // On Posix, named shared memory uses a file on disk. This file
  // could be lying around from previous crashes which could cause
  // GetServiceProcessPid to lie. On Windows, we use a named event so we
  // don't have this issue. Until we have a more stable shared memory
  // implementation on Posix, this check will only execute on Windows.
  ASSERT_FALSE(GetServiceProcessData(&version, &pid));
#endif  // defined(OS_WIN)
  ServiceProcessState state;
  ASSERT_TRUE(state.Initialize());
  ASSERT_TRUE(GetServiceProcessData(&version, &pid));
  ASSERT_EQ(base::GetCurrentProcId(), pid);
}

TEST_F(ServiceProcessStateTest, MAYBE_ForceShutdown) {
  base::ProcessHandle handle = SpawnChild("ServiceProcessStateTestShutdown",
                                          true);
  ASSERT_TRUE(handle);
  for (int i = 0; !CheckServiceProcessReady() && i < 10; ++i) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout_ms());
  }
  ASSERT_TRUE(CheckServiceProcessReady());
  std::string version;
  base::ProcessId pid;
  ASSERT_TRUE(GetServiceProcessData(&version, &pid));
  ASSERT_TRUE(ForceServiceProcessShutdown(version, pid));
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCodeWithTimeout(handle,
      &exit_code, TestTimeouts::action_max_timeout_ms()));
  base::CloseProcessHandle(handle);
  ASSERT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestSingleton) {
  ServiceProcessState state;
  EXPECT_FALSE(state.Initialize());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestReadyTrue) {
  EXPECT_TRUE(CheckServiceProcessReady());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestReadyFalse) {
  EXPECT_FALSE(CheckServiceProcessReady());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestShutdown) {
  MessageLoop message_loop;
  message_loop.set_thread_name("ServiceProcessStateTestShutdownMainThread");
  base::Thread io_thread_("ServiceProcessStateTestShutdownIOThread");
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  EXPECT_TRUE(io_thread_.StartWithOptions(options));
  ServiceProcessState state;
  EXPECT_TRUE(state.Initialize());
  EXPECT_TRUE(state.SignalReady(io_thread_.message_loop_proxy(),
                                base::Bind(&ShutdownTask,
                                           MessageLoop::current())));
  message_loop.PostDelayedTask(FROM_HERE,
                               new MessageLoop::QuitTask(),
                               TestTimeouts::action_max_timeout_ms());
  EXPECT_FALSE(g_good_shutdown);
  message_loop.Run();
  EXPECT_TRUE(g_good_shutdown);
  return 0;
}

#else  // !OS_MACOSX

#include <CoreFoundation/CoreFoundation.h>

#include <launch.h>
#include <sys/stat.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/mac/launchd.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(dmaclach): Write this in terms of a real mock.
// http://crbug.com/76923
class MockLaunchd : public Launchd {
 public:
  MockLaunchd(const FilePath& file, MessageLoop* loop)
      : file_(file),
        message_loop_(loop),
        restart_called_(false),
        remove_called_(false),
        checkin_called_(false),
        write_called_(false),
        delete_called_(false) {
  }
  virtual ~MockLaunchd() { }

  virtual CFDictionaryRef CopyExports() OVERRIDE {
    ADD_FAILURE();
    return NULL;
  }

  virtual CFDictionaryRef CopyJobDictionary(CFStringRef label) OVERRIDE {
    ADD_FAILURE();
    return NULL;
  }

  virtual CFDictionaryRef CopyDictionaryByCheckingIn(CFErrorRef* error)
      OVERRIDE {
    checkin_called_ = true;
    CFStringRef program = CFSTR(LAUNCH_JOBKEY_PROGRAM);
    CFStringRef program_args = CFSTR(LAUNCH_JOBKEY_PROGRAMARGUMENTS);
    const void *keys[] = { program, program_args };
    base::mac::ScopedCFTypeRef<CFStringRef> path(
        base::SysUTF8ToCFStringRef(file_.value()));
    const void *array_values[] = { path.get() };
    base::mac::ScopedCFTypeRef<CFArrayRef> args(
        CFArrayCreate(kCFAllocatorDefault,
                      array_values,
                      1,
                      &kCFTypeArrayCallBacks));
    const void *values[] = { path, args };
    return CFDictionaryCreate(kCFAllocatorDefault,
                              keys,
                              values,
                              arraysize(keys),
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
  }

  virtual bool RemoveJob(CFStringRef label, CFErrorRef* error) OVERRIDE {
    remove_called_ = true;
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    return true;
  }

  virtual bool RestartJob(Domain domain,
                          Type type,
                          CFStringRef name,
                          CFStringRef session_type) OVERRIDE {
    restart_called_ = true;
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    return true;
  }

  virtual CFMutableDictionaryRef CreatePlistFromFile(
      Domain domain,
      Type type,
      CFStringRef name) OVERRIDE {
    base::mac::ScopedCFTypeRef<CFDictionaryRef> dict(
        CopyDictionaryByCheckingIn(NULL));
    return CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict);
  }

  virtual bool WritePlistToFile(Domain domain,
                                Type type,
                                CFStringRef name,
                                CFDictionaryRef dict) OVERRIDE {
    write_called_ = true;
    return true;
  }

  virtual bool DeletePlist(Domain domain,
                           Type type,
                           CFStringRef name) OVERRIDE {
    delete_called_ = true;
    return true;
  }

  bool restart_called() const { return restart_called_; }
  bool remove_called() const { return remove_called_; }
  bool checkin_called() const { return checkin_called_; }
  bool write_called() const { return write_called_; }
  bool delete_called() const { return delete_called_; }

 private:
  FilePath file_;
  MessageLoop* message_loop_;
  bool restart_called_;
  bool remove_called_;
  bool checkin_called_;
  bool write_called_;
  bool delete_called_;
};

class ServiceProcessStateFileManipulationTest : public ::testing::Test {
 protected:
  ServiceProcessStateFileManipulationTest()
      : io_thread_("ServiceProcessStateFileManipulationTest_IO") {
  }
  virtual ~ServiceProcessStateFileManipulationTest() { }

  virtual void SetUp() {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(MakeABundle(GetTempDirPath(),
                            "Test",
                            &bundle_path_,
                            &executable_path_));
    mock_launchd_.reset(new MockLaunchd(executable_path_, &loop_));
    scoped_launchd_instance_.reset(
        new Launchd::ScopedInstance(mock_launchd_.get()));
    ASSERT_TRUE(service_process_state_.Initialize());
    ASSERT_TRUE(service_process_state_.SignalReady(
        io_thread_.message_loop_proxy(),
        base::Closure()));
    loop_.PostDelayedTask(FROM_HERE,
                          new MessageLoop::QuitTask,
                          TestTimeouts::action_max_timeout_ms());
  }

  bool MakeABundle(const FilePath& dst,
                   const std::string& name,
                   FilePath* bundle_root,
                   FilePath* executable) {
    *bundle_root = dst.Append(name + std::string(".app"));
    FilePath contents = bundle_root->AppendASCII("Contents");
    FilePath mac_os = contents.AppendASCII("MacOS");
    *executable = mac_os.Append(name);
    FilePath info_plist = contents.Append("Info.plist");

    if (!file_util::CreateDirectory(mac_os)) {
      return false;
    }
    const char *data = "#! testbundle\n";
    int len = strlen(data);
    if (file_util::WriteFile(*executable, data, len) != len) {
      return false;
    }
    if (chmod(executable->value().c_str(), 0555) != 0) {
      return false;
    }

    const char* info_plist_format =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      "<plist version=\"1.0\">\n"
      "<dict>\n"
      "  <key>CFBundleDevelopmentRegion</key>\n"
      "  <string>English</string>\n"
      "  <key>CFBundleIdentifier</key>\n"
      "  <string>com.test.%s</string>\n"
      "  <key>CFBundleInfoDictionaryVersion</key>\n"
      "  <string>6.0</string>\n"
      "  <key>CFBundleExecutable</key>\n"
      "  <string>%s</string>\n"
      "  <key>CFBundleVersion</key>\n"
      "  <string>1</string>\n"
      "</dict>\n"
      "</plist>\n";
    std::string info_plist_data = base::StringPrintf(info_plist_format,
                                                     name.c_str(),
                                                     name.c_str());
    len = info_plist_data.length();
    if (file_util::WriteFile(info_plist, info_plist_data.c_str(), len) != len) {
      return false;
    }
    const UInt8* bundle_root_path =
        reinterpret_cast<const UInt8*>(bundle_root->value().c_str());
    base::mac::ScopedCFTypeRef<CFURLRef> url(
      CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                              bundle_root_path,
                                              bundle_root->value().length(),
                                              true));
    base::mac::ScopedCFTypeRef<CFBundleRef> bundle(
        CFBundleCreate(kCFAllocatorDefault, url));
    return bundle.get();
  }

  const MockLaunchd* mock_launchd() const { return mock_launchd_.get(); }
  const FilePath& executable_path() const { return executable_path_; }
  const FilePath& bundle_path() const { return bundle_path_; }
  const FilePath& GetTempDirPath() const { return temp_dir_.path(); }

  base::MessageLoopProxy* GetIOMessageLoopProxy() {
    return io_thread_.message_loop_proxy().get();
  }
  void Run() { loop_.Run(); }

 private:
  ScopedTempDir temp_dir_;
  MessageLoopForUI loop_;
  base::Thread io_thread_;
  FilePath executable_path_, bundle_path_;
  scoped_ptr<MockLaunchd> mock_launchd_;
  scoped_ptr<Launchd::ScopedInstance> scoped_launchd_instance_;
  ServiceProcessState service_process_state_;
};

void DeleteFunc(const FilePath& file) {
  EXPECT_TRUE(file_util::Delete(file, true));
}

void MoveFunc(const FilePath& from, const FilePath& to) {
  EXPECT_TRUE(file_util::Move(from, to));
}

void ChangeAttr(const FilePath& from, int mode) {
  EXPECT_EQ(chmod(from.value().c_str(), mode), 0);
}

class ScopedAttributesRestorer {
 public:
  ScopedAttributesRestorer(const FilePath& path, int mode)
      : path_(path), mode_(mode) {
  }
  ~ScopedAttributesRestorer() {
    ChangeAttr(path_, mode_);
  }
 private:
  FilePath path_;
  int mode_;
};

void TrashFunc(const FilePath& src) {
  FSRef path_ref;
  FSRef new_path_ref;
  EXPECT_TRUE(base::mac::FSRefFromPath(src.value(), &path_ref));
  OSStatus status = FSMoveObjectToTrashSync(&path_ref,
                                            &new_path_ref,
                                            kFSFileOperationDefaultOptions);
  EXPECT_EQ(status, noErr)  << "FSMoveObjectToTrashSync " << status;
}

TEST_F(ServiceProcessStateFileManipulationTest, VerifyLaunchD) {
  // There have been problems where launchd has gotten into a bad state, usually
  // because something had deleted all the files in /tmp. launchd depends on
  // a Unix Domain Socket that it creates at /tmp/launchd*/sock.
  // The symptom of this problem is that the service process connect fails
  // on Mac and "launch_msg(): Socket is not connected" appears.
  // This test is designed to make sure that launchd is working.
  // http://crbug/75518

  CommandLine cl(FilePath("/bin/launchctl"));
  cl.AppendArg("list");
  cl.AppendArg("com.apple.launchctl.Aqua");

  std::string output;
  int exit_code = -1;
  ASSERT_TRUE(base::GetAppOutputWithExitCode(cl, &output, &exit_code)
              && exit_code == 0)
      << " exit_code:" << exit_code << " " << output;
}

TEST_F(ServiceProcessStateFileManipulationTest, DeleteFile) {
  GetIOMessageLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteFunc, executable_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, DeleteBundle) {
  GetIOMessageLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteFunc, bundle_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, MoveBundle) {
  FilePath new_loc = GetTempDirPath().AppendASCII("MoveBundle");
  GetIOMessageLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(&MoveFunc, bundle_path(), new_loc));
  Run();
  ASSERT_TRUE(mock_launchd()->restart_called());
  ASSERT_TRUE(mock_launchd()->write_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, MoveFile) {
  FilePath new_loc = GetTempDirPath().AppendASCII("MoveFile");
  GetIOMessageLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(&MoveFunc, executable_path(), new_loc));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, TrashBundle) {
  FSRef bundle_ref;
  ASSERT_TRUE(base::mac::FSRefFromPath(bundle_path().value(), &bundle_ref));
  GetIOMessageLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(&TrashFunc, bundle_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
  std::string path(base::mac::PathFromFSRef(bundle_ref));
  FilePath file_path(path);
  ASSERT_TRUE(file_util::Delete(file_path, true));
}

TEST_F(ServiceProcessStateFileManipulationTest, ChangeAttr) {
  ScopedAttributesRestorer restorer(bundle_path(), 0777);
  GetIOMessageLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(&ChangeAttr, bundle_path(), 0222));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

#endif  // !OS_MACOSX
