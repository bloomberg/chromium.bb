// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/mock_launchd.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/mac/launchd.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
bool MockLaunchd::MakeABundle(const FilePath& dst,
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
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
          "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
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

CFDictionaryRef MockLaunchd::CopyExports() {
  ADD_FAILURE();
  return NULL;
}

CFDictionaryRef MockLaunchd::CopyJobDictionary(CFStringRef label) {
  ADD_FAILURE();
  return NULL;
}

CFDictionaryRef MockLaunchd::CopyDictionaryByCheckingIn(CFErrorRef* error) {
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

bool MockLaunchd::RemoveJob(CFStringRef label, CFErrorRef* error) {
  remove_called_ = true;
  message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  return true;
}

bool MockLaunchd::RestartJob(Domain domain,
                             Type type,
                             CFStringRef name,
                             CFStringRef session_type) {
  restart_called_ = true;
  message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  return true;
}

CFMutableDictionaryRef MockLaunchd::CreatePlistFromFile(
    Domain domain,
    Type type,
    CFStringRef name)  {
  base::mac::ScopedCFTypeRef<CFDictionaryRef> dict(
      CopyDictionaryByCheckingIn(NULL));
  return CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict);
}

bool MockLaunchd::WritePlistToFile(Domain domain,
                                   Type type,
                                   CFStringRef name,
                                   CFDictionaryRef dict) {
  write_called_ = true;
  return true;
}

bool MockLaunchd::DeletePlist(Domain domain,
                              Type type,
                              CFStringRef name) {
  delete_called_ = true;
  return true;
}
