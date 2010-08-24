// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_mac.h"

#include "base/debug_util.h"

#import <Cocoa/Cocoa.h>
extern "C" {
#include <sandbox.h>
}
#include <sys/param.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/mac_util.h"
#include "base/rand_util_c.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "unicode/uchar.h"

namespace {

// Try to escape |c| as a "SingleEscapeCharacter" (\n, etc).  If successful,
// returns true and appends the escape sequence to |dst|.
bool EscapeSingleChar(char c, std::string* dst) {
  const char *append = NULL;
  switch (c) {
    case '\b':
      append = "\\b";
      break;
    case '\f':
      append = "\\f";
      break;
    case '\n':
      append = "\\n";
      break;
    case '\r':
      append = "\\r";
      break;
    case '\t':
      append = "\\t";
      break;
    case '\\':
      append = "\\\\";
      break;
    case '"':
      append = "\\\"";
      break;
  }

  if (!append) {
    return false;
  }

  dst->append(append);
  return true;
}

}  // namespace

namespace sandbox {

// Escape |str_utf8| for use in a plain string variable in a sandbox
// configuraton file.  On return |dst| is set to the utf-8 encoded quoted
// output.
// Returns: true on success, false otherwise.
bool QuotePlainString(const std::string& str_utf8, std::string* dst) {
  dst->clear();

  const char* src = str_utf8.c_str();
  int32_t length = str_utf8.length();
  int32_t position = 0;
  while (position < length) {
    UChar32 c;
    U8_NEXT(src, position, length, c);  // Macro increments |position|.
    DCHECK_GE(c, 0);
    if (c < 0)
      return false;

    if (c < 128) {  // EscapeSingleChar only handles ASCII.
      char as_char = static_cast<char>(c);
      if (EscapeSingleChar(as_char, dst)) {
        continue;
      }
    }

    if (c < 32 || c > 126) {
      // Any characters that aren't printable ASCII get the \u treatment.
      unsigned int as_uint = static_cast<unsigned int>(c);
      StringAppendF(dst, "\\u%04X", as_uint);
      continue;
    }

    // If we got here we know that the character in question is strictly
    // in the ASCII range so there's no need to do any kind of encoding
    // conversion.
    dst->push_back(static_cast<char>(c));
  }
  return true;
}

// Escape |str_utf8| for use in a regex literal in a sandbox
// configuraton file.  On return |dst| is set to the utf-8 encoded quoted
// output.
//
// The implementation of this function is based on empirical testing of the
// OS X sandbox on 10.5.8 & 10.6.2 which is undocumented and subject to change.
//
// Note: If str_utf8 contains any characters < 32 || >125 then the function
// fails and false is returned.
//
// Returns: true on success, false otherwise.
bool QuoteStringForRegex(const std::string& str_utf8, std::string* dst) {
  // Characters with special meanings in sandbox profile syntax.
  const char regex_special_chars[] = {
    '\\',

    // Metacharacters
    '^',
    '.',
    '[',
    ']',
    '$',
    '(',
    ')',
    '|',

    // Quantifiers
    '*',
    '+',
    '?',
    '{',
    '}',
  };

  // Anchor regex at start of path.
  dst->assign("^");

  const char* src = str_utf8.c_str();
  int32_t length = str_utf8.length();
  int32_t position = 0;
  while (position < length) {
    UChar32 c;
    U8_NEXT(src, position, length, c);  // Macro increments |position|.
    DCHECK_GE(c, 0);
    if (c < 0)
      return false;

    // The Mac sandbox regex parser only handles printable ASCII characters.
    // 33 >= c <= 126
    if (c < 32 || c > 125) {
      return false;
    }

    for (size_t i = 0; i < arraysize(regex_special_chars); ++i) {
      if (c == regex_special_chars[i]) {
        dst->push_back('\\');
        break;
      }
    }

    dst->push_back(static_cast<char>(c));
  }

  // Make sure last element of path is interpreted as a directory. Leaving this
  // off would allow access to files if they start with the same name as the
  // directory.
  dst->append("(/|$)");

  return true;
}

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6.0
void SandboxWarmup() {
  base::ScopedNSAutoreleasePool scoped_pool;

  { // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
    scoped_cftyperef<CGColorSpaceRef> rgb_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

    // Allocate a 1x1 image.
    char data[4];
    scoped_cftyperef<CGContextRef> context(
        CGBitmapContextCreate(data, 1, 1, 8, 1 * 4,
                              rgb_colorspace,
                              kCGImageAlphaPremultipliedFirst |
                                  kCGBitmapByteOrder32Host));

    // Load in the color profiles we'll need (as a side effect).
    (void) mac_util::GetSRGBColorSpace();
    (void) mac_util::GetSystemColorSpace();

    // CGColorSpaceCreateSystemDefaultCMYK - 10.6
    scoped_cftyperef<CGColorSpaceRef> cmyk_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK));
  }

  { // [-NSColor colorUsingColorSpaceName] - 10.5.6
    NSColor* color = [NSColor controlTextColor];
    [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  }

  { // localtime() - 10.5.6
    time_t tv = {0};
    localtime(&tv);
  }

  { // Gestalt() tries to read /System/Library/CoreServices/SystemVersion.plist
    // on 10.5.6
    int32 tmp;
    base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
  }

  {  // CGImageSourceGetStatus() - 10.6
     // Create a png with just enough data to get everything warmed up...
    char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    NSData* data = [NSData dataWithBytes:png_header
                                  length:arraysize(png_header)];
    scoped_cftyperef<CGImageSourceRef> img(
        CGImageSourceCreateWithData((CFDataRef)data,
        NULL));
    CGImageSourceGetStatus(img);
  }

  { // Native Client access to /dev/random.
    GetUrandomFD();
  }
}

// Turns on the OS X sandbox for this process.
bool EnableSandbox(SandboxProcessType sandbox_type,
                   const FilePath& allowed_dir) {
  // Sanity - currently only SANDBOX_TYPE_UTILITY supports a directory being
  // passed in.
  if (sandbox_type != SANDBOX_TYPE_UTILITY) {
    DCHECK(allowed_dir.empty())
        << "Only SANDBOX_TYPE_UTILITY allows a custom directory parameter.";
  }
  // We use a custom sandbox definition file to lock things down as
  // tightly as possible.
  // TODO(jeremy): Look at using include syntax to unify common parts of sandbox
  // definition files.
  NSString* sandbox_config_filename = nil;
  switch (sandbox_type) {
    case SANDBOX_TYPE_RENDERER:
      sandbox_config_filename = @"renderer";
      break;
    case SANDBOX_TYPE_WORKER:
      sandbox_config_filename = @"worker";
      break;
    case SANDBOX_TYPE_UTILITY:
      sandbox_config_filename = @"utility";
      break;
    case SANDBOX_TYPE_NACL_LOADER:
      // The Native Client loader is used for safeguarding the user's
      // untrusted code within Native Client.
      sandbox_config_filename = @"nacl_loader";
      break;
    default:
      NOTREACHED();
      return false;
  }

  // Read in the sandbox profile and the common prefix file.
  NSString* common_sandbox_prefix_path =
      [mac_util::MainAppBundle() pathForResource:@"common"
                                          ofType:@"sb"];
  NSString* common_sandbox_prefix_data =
      [NSString stringWithContentsOfFile:common_sandbox_prefix_path
                                encoding:NSUTF8StringEncoding
                                   error:NULL];

  if (!common_sandbox_prefix_data) {
    LOG(FATAL) << "Failed to find the sandbox profile on disk "
               << [common_sandbox_prefix_path fileSystemRepresentation];
    return false;
  }

  NSString* sandbox_profile_path =
      [mac_util::MainAppBundle() pathForResource:sandbox_config_filename
                                          ofType:@"sb"];
  NSString* sandbox_data =
      [NSString stringWithContentsOfFile:sandbox_profile_path
                                 encoding:NSUTF8StringEncoding
                                    error:NULL];

  if (!sandbox_data) {
    LOG(FATAL) << "Failed to find the sandbox profile on disk "
               << [sandbox_profile_path fileSystemRepresentation];
    return false;
  }

  // Prefix sandbox_data with common_sandbox_prefix_data.
  sandbox_data =
      [common_sandbox_prefix_data stringByAppendingString:sandbox_data];

  // Enable verbose logging if enabled on the command line. (See common.sb
  // for details).
  const CommandLine *command_line = CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(switches::kEnableSandboxLogging);
  if (enable_logging) {
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@";ENABLE_LOGGING"
                                  withString:@""];
  }

  // Get the OS version.
  int32 major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version,
      &minor_version, &bugfix_version);
  bool snow_leopard_or_higher =
      (major_version > 10 || (major_version == 10 && minor_version >= 6));

  // Without this, the sandbox will print a message to the system log every
  // time it denies a request.  This floods the console with useless spew. The
  // (with no-log) syntax is only supported on 10.6+
  if (snow_leopard_or_higher && !enable_logging) {
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@"DISABLE_SANDBOX_DENIAL_LOGGING"
                                  withString:@"(with no-log)"];
  } else {
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@"DISABLE_SANDBOX_DENIAL_LOGGING"
                                  withString:@""];
  }

  if (!allowed_dir.empty()) {
    // The sandbox only understands "real" paths.  This resolving step is
    // needed so the caller doesn't need to worry about things like /var
    // being a link to /private/var (like in the paths CreateNewTempDirectory()
    // returns).
    FilePath allowed_dir_canonical(allowed_dir);
    GetCanonicalSandboxPath(&allowed_dir_canonical);

    std::string allowed_dir_escaped;
    if (!QuoteStringForRegex(allowed_dir_canonical.value(),
                             &allowed_dir_escaped)) {
      LOG(FATAL) << "Regex string quoting failed " << allowed_dir.value();
      return false;
    }
    NSString* allowed_dir_escaped_ns = base::SysUTF8ToNSString(
        allowed_dir_escaped.c_str());
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@";ENABLE_DIRECTORY_ACCESS"
                                  withString:@""];
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@"DIR_TO_ALLOW_ACCESS"
                                  withString:allowed_dir_escaped_ns];

  }

  if (snow_leopard_or_higher) {
    // 10.6-only Sandbox rules.
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@";10.6_ONLY"
                                  withString:@""];
    // Splice the path of the user's home directory into the sandbox profile
    // (see renderer.sb for details).
    // This code is in the 10.6-only block because the sandbox syntax we use
    // for this "subdir" is only supported on 10.6.
    // If we ever need this on pre-10.6 OSs then we'll have to rethink the
    // surrounding sandbox syntax.
    std::string home_dir = base::SysNSStringToUTF8(NSHomeDirectory());

    FilePath home_dir_canonical(home_dir);
    GetCanonicalSandboxPath(&home_dir_canonical);

    std::string home_dir_escaped;
    if (!QuotePlainString(home_dir_canonical.value(), &home_dir_escaped)) {
      LOG(FATAL) << "Sandbox string quoting failed";
      return false;
    }
    NSString* home_dir_escaped_ns = base::SysUTF8ToNSString(home_dir_escaped);
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@"USER_HOMEDIR"
                                  withString:home_dir_escaped_ns];
  } else if (major_version == 10 && minor_version < 6) {
    // Sandbox rules only for versions before 10.6.
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@";BEFORE_10.6"
                                  withString:@""];
  }

  char* error_buff = NULL;
  int error = sandbox_init([sandbox_data UTF8String], 0, &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  LOG_IF(FATAL, !success) << "Failed to initialize sandbox: "
                          << error
                          << " "
                          << error_buff;
  sandbox_free_error(error_buff);
  return success;
}

void GetCanonicalSandboxPath(FilePath* path) {
  int fd = HANDLE_EINTR(open(path->value().c_str(), O_RDONLY));
  if (fd < 0) {
    PLOG(FATAL) << "GetCanonicalSandboxPath() failed for: "
                << path->value();
    return;
  }
  file_util::ScopedFD file_closer(&fd);

  FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd, F_GETPATH, canonical_path)) != 0) {
    PLOG(FATAL) << "GetCanonicalSandboxPath() failed for: "
                << path->value();
    return;
  }

  *path = FilePath(canonical_path);
}

}  // namespace sandbox
