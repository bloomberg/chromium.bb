// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "crypto/openssl_util.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "services/service_manager/sandbox/mac/audio.sb.h"
#include "services/service_manager/sandbox/mac/cdm.sb.h"
#include "services/service_manager/sandbox/mac/common_v2.sb.h"
#include "services/service_manager/sandbox/mac/gpu_v2.sb.h"
#include "services/service_manager/sandbox/mac/nacl_loader.sb.h"
#include "services/service_manager/sandbox/mac/pdf_compositor.sb.h"
#include "services/service_manager/sandbox/mac/ppapi_v2.sb.h"
#include "services/service_manager/sandbox/mac/renderer_v2.sb.h"
#include "services/service_manager/sandbox/mac/utility.sb.h"
#include "services/service_manager/sandbox/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#import "ui/base/clipboard/clipboard_util_mac.h"

namespace content {
namespace {

// crbug.com/740009: This allows the unit test to cleanup temporary directories,
// and is safe since this is only a unit test.
constexpr char kTempDirSuffix[] =
    "(allow file* (subpath \"/private/var/folders\"))";
constexpr char kClipboardArg[] = "pasteboard-name";

class SandboxMacTest : public base::MultiProcessTest {
 protected:
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine cl = MultiProcessTest::MakeCmdLine(procname);
    cl.AppendArg(
        base::StringPrintf("%s%d", sandbox::switches::kSeatbeltClient, pipe_));
    return cl;
  }

  std::string ProfileForSandbox(const std::string& sandbox_profile) {
    return std::string(service_manager::kSeatbeltPolicyString_common_v2) +
           sandbox_profile + kTempDirSuffix;
  }

  void ExecuteWithParams(const std::string& procname,
                         const std::string& sub_profile,
                         void (*setup)(sandbox::SeatbeltExecClient*)) {
    std::string profile = ProfileForSandbox(sub_profile);
    sandbox::SeatbeltExecClient client;
    client.SetProfile(profile);
    setup(&client);

    pipe_ = client.GetReadFD();
    ASSERT_GE(pipe_, 0);

    base::LaunchOptions options;
    options.fds_to_remap.push_back(std::make_pair(pipe_, pipe_));

    base::Process process = SpawnChildWithOptions(procname, options);
    ASSERT_TRUE(process.IsValid());
    ASSERT_TRUE(client.SendProfile());

    int rv = -1;
    ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_timeout(), &rv));
    EXPECT_EQ(0, rv);
  }

  void ExecuteInAudioSandbox(const std::string& procname) {
    ExecuteWithParams(procname, service_manager::kSeatbeltPolicyString_audio,
                      &content::SetupCommonSandboxParameters);
  }

  void ExecuteInCDMSandbox(const std::string& procname) {
    ExecuteWithParams(procname, service_manager::kSeatbeltPolicyString_cdm,
                      &content::SetupCDMSandboxParameters);
  }

  void ExecuteInGPUSandbox(const std::string& procname) {
    ExecuteWithParams(procname, service_manager::kSeatbeltPolicyString_gpu_v2,
                      &content::SetupCommonSandboxParameters);
  }

  void ExecuteInNaClSandbox(const std::string& procname) {
    ExecuteWithParams(procname,
                      service_manager::kSeatbeltPolicyString_nacl_loader,
                      &content::SetupCommonSandboxParameters);
  }

  void ExecuteInPDFSandbox(const std::string& procname) {
    ExecuteWithParams(procname,
                      service_manager::kSeatbeltPolicyString_pdf_compositor,
                      &content::SetupCommonSandboxParameters);
  }

  void ExecuteInPpapiSandbox(const std::string& procname) {
    ExecuteWithParams(procname, service_manager::kSeatbeltPolicyString_ppapi_v2,
                      &content::SetupPPAPISandboxParameters);
  }

  void ExecuteInRendererSandbox(const std::string& procname) {
    ExecuteWithParams(procname,
                      service_manager::kSeatbeltPolicyString_renderer_v2,
                      &content::SetupCommonSandboxParameters);
  }

  void ExecuteInUtilitySandbox(const std::string& procname) {
    ExecuteWithParams(procname, service_manager::kSeatbeltPolicyString_utility,
                      [](sandbox::SeatbeltExecClient* client) -> void {
                        content::SetupUtilitySandboxParameters(
                            client, *base::CommandLine::ForCurrentProcess());
                      });
  }

  void ExecuteInAllSandboxTypes(const std::string& multiprocess_main,
                                base::RepeatingClosure after_each) {
    using ExecuteFuncT = void (SandboxMacTest::*)(const std::string&);
    constexpr ExecuteFuncT kExecuteFuncs[] = {
        &SandboxMacTest::ExecuteInAudioSandbox,
        &SandboxMacTest::ExecuteInCDMSandbox,
        &SandboxMacTest::ExecuteInGPUSandbox,
        &SandboxMacTest::ExecuteInNaClSandbox,
        &SandboxMacTest::ExecuteInPDFSandbox,
        &SandboxMacTest::ExecuteInPpapiSandbox,
        &SandboxMacTest::ExecuteInRendererSandbox,
        &SandboxMacTest::ExecuteInUtilitySandbox,
    };

    for (ExecuteFuncT execute_func : kExecuteFuncs) {
      (this->*execute_func)(multiprocess_main);
      if (!after_each.is_null()) {
        after_each.Run();
      }
    }
  }

  int pipe_{0};
};

class SandboxMacClipboardTest : public SandboxMacTest {
 protected:
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine cl = SandboxMacTest::MakeCmdLine(procname);
    cl.AppendSwitchASCII(kClipboardArg, pasteboard_name_);
    return cl;
  }

  std::string pasteboard_name_{};
};

void CheckCreateSeatbeltServer() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& argv = cl->argv();
  std::vector<char*> argv_cstr(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    argv_cstr[i] = const_cast<char*>(argv[i].c_str());
  }
  auto result = sandbox::SeatbeltExecServer::CreateFromArguments(
      argv_cstr[0], argv_cstr.size(), argv_cstr.data());

  CHECK(result.sandbox_required);
  CHECK(result.server);
  CHECK(result.server->InitializeSandbox());
}

}  // namespace

MULTIPROCESS_TEST_MAIN(RendererWriteProcess) {
  CheckCreateSeatbeltServer();

  // Test that the renderer cannot write to the home directory.
  NSString* test_file = [NSHomeDirectory()
      stringByAppendingPathComponent:@"e539dd6f-6b38-4f6a-af2c-809a5ea96e1c"];
  int fd = HANDLE_EINTR(
      open(base::SysNSStringToUTF8(test_file).c_str(), O_CREAT | O_RDWR));
  CHECK(-1 == fd);
  CHECK_EQ(errno, EPERM);

  return 0;
}

TEST_F(SandboxMacTest, RendererCannotWriteHomeDir) {
  ExecuteInRendererSandbox("RendererWriteProcess");
}

MULTIPROCESS_TEST_MAIN(ClipboardAccessProcess) {
  CheckCreateSeatbeltServer();

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  std::string pasteboard_name = cl->GetSwitchValueASCII(kClipboardArg);
  CHECK(!pasteboard_name.empty());
  CHECK([NSPasteboard pasteboardWithName:base::SysUTF8ToNSString(
                                             pasteboard_name)] == nil);
  CHECK([NSPasteboard generalPasteboard] == nil);

  return 0;
}

TEST_F(SandboxMacClipboardTest, ClipboardAccess) {
  scoped_refptr<ui::UniquePasteboard> pb = new ui::UniquePasteboard;
  ASSERT_TRUE(pb->get());
  EXPECT_EQ([[pb->get() types] count], 0U);

  pasteboard_name_ = base::SysNSStringToUTF8([pb->get() name]);

  ExecuteInAllSandboxTypes("ClipboardAccessProcess",
                           base::BindRepeating(
                               [](scoped_refptr<ui::UniquePasteboard> pb) {
                                 ASSERT_EQ([[pb->get() types] count], 0U);
                               },
                               pb));
}

MULTIPROCESS_TEST_MAIN(SSLProcess) {
  CheckCreateSeatbeltServer();

  crypto::EnsureOpenSSLInit();
  // Ensure that RAND_bytes is functional within the sandbox.
  uint8_t byte;
  CHECK(RAND_bytes(&byte, 1) == 1);
  return 0;
}

TEST_F(SandboxMacTest, SSLInitTest) {
  ExecuteInAllSandboxTypes("SSLProcess", base::RepeatingClosure());
}

}  // namespace content
