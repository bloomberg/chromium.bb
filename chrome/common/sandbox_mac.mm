// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>

extern "C" {
#include <sandbox.h>
}
#include <signal.h>
#include <sys/param.h>

#include "app/gfx/gl/gl_context.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "base/rand_util_c.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_application_mac.h"
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


// static
bool Sandbox::QuotePlainString(const std::string& src_utf8, std::string* dst) {
  dst->clear();

  const char* src = src_utf8.c_str();
  int32_t length = src_utf8.length();
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
      base::StringAppendF(dst, "\\u%04X", as_uint);
      continue;
    }

    // If we got here we know that the character in question is strictly
    // in the ASCII range so there's no need to do any kind of encoding
    // conversion.
    dst->push_back(static_cast<char>(c));
  }
  return true;
}

// static
bool Sandbox::QuoteStringForRegex(const std::string& str_utf8,
                                  std::string* dst) {
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

// static
void Sandbox::SandboxWarmup(SandboxProcessType sandbox_type) {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  { // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
    base::mac::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

    // Allocate a 1x1 image.
    char data[4];
    base::mac::ScopedCFTypeRef<CGContextRef> context(
        CGBitmapContextCreate(data, 1, 1, 8, 1 * 4,
                              rgb_colorspace,
                              kCGImageAlphaPremultipliedFirst |
                                  kCGBitmapByteOrder32Host));

    // Load in the color profiles we'll need (as a side effect).
    (void) base::mac::GetSRGBColorSpace();
    (void) base::mac::GetSystemColorSpace();

    // CGColorSpaceCreateSystemDefaultCMYK - 10.6
    base::mac::ScopedCFTypeRef<CGColorSpaceRef> cmyk_colorspace(
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
    base::mac::ScopedCFTypeRef<CGImageSourceRef> img(
        CGImageSourceCreateWithData((CFDataRef)data,
        NULL));
    CGImageSourceGetStatus(img);
  }

  {
    // Allow access to /dev/urandom.
    GetUrandomFD();
  }

  // Process-type dependent warm-up.
  switch (sandbox_type) {
    case SANDBOX_TYPE_GPU:
      {  // GPU-related stuff is very slow without this, probably because
         // the sandbox prevents loading graphics drivers or some such.
         CGLPixelFormatAttribute attribs[] = { (CGLPixelFormatAttribute)0 };
         CGLPixelFormatObj format;
         GLint n;
         CGLChoosePixelFormat(attribs, &format, &n);
         if (format)
           CGLReleasePixelFormat(format);
      }

      {
         // Preload either the desktop GL or the osmesa so, depending on the
         // --use-gl flag.
         gfx::GLContext::InitializeOneOff();
      }
      break;

    default:
      // To shut up a gcc warning.
      break;
  }
}

// static
NSString* Sandbox::BuildAllowDirectoryAccessSandboxString(
    const FilePath& allowed_dir,
    SandboxVariableSubstitions* substitutions) {
  // A whitelist is used to determine which directories can be statted
  // This means that in the case of an /a/b/c/d/ directory, we may be able to
  // stat the leaf directory, but not it's parent.
  // The extension code in Chrome calls realpath() which fails if it can't call
  // stat() on one of the parent directories in the path.
  // The solution to this is to allow statting the parent directories themselves
  // but not their contents.  We need to add a separate rule for each parent
  // directory.

  // The sandbox only understands "real" paths.  This resolving step is
  // needed so the caller doesn't need to worry about things like /var
  // being a link to /private/var (like in the paths CreateNewTempDirectory()
  // returns).
  FilePath allowed_dir_canonical(allowed_dir);
  GetCanonicalSandboxPath(&allowed_dir_canonical);

  // Collect a list of all parent directories.
  FilePath last_path = allowed_dir_canonical;
  std::vector<FilePath> subpaths;
  for (FilePath path = allowed_dir_canonical.DirName();
       path.value() != last_path.value();
       path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through all parents and allow stat() on them explicitly.
  NSString* sandbox_command = @"(allow file-read-metadata ";
  for (std::vector<FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend();
       ++i) {
    std::string subdir_escaped;
    if (!QuotePlainString(i->value(), &subdir_escaped)) {
      LOG(FATAL) << "String quoting failed " << i->value();
      return nil;
    }

    NSString* subdir_escaped_ns =
        base::SysUTF8ToNSString(subdir_escaped.c_str());
    sandbox_command =
        [sandbox_command stringByAppendingFormat:@"(literal \"%@\")",
            subdir_escaped_ns];
  }

  // Finally append the leaf directory.  Unlike it's parents (for which only
  // stat() should be allowed), the leaf directory needs full access.
  (*substitutions)["ALLOWED_DIR"] =
      SandboxSubstring(allowed_dir_canonical.value(),
                       SandboxSubstring::REGEX);
  sandbox_command =
      [sandbox_command
          stringByAppendingString:@") (allow file-read* file-write*"
                                   " (regex #\"@ALLOWED_DIR@\") )"];
  return sandbox_command;
}

// Load the appropriate template for the given sandbox type.
// Returns the template as an NSString or nil on error.
NSString* LoadSandboxTemplate(Sandbox::SandboxProcessType sandbox_type) {
  // We use a custom sandbox definition file to lock things down as
  // tightly as possible.
  NSString* sandbox_config_filename = nil;
  switch (sandbox_type) {
    case Sandbox::SANDBOX_TYPE_RENDERER:
      sandbox_config_filename = @"renderer";
      break;
    case Sandbox::SANDBOX_TYPE_WORKER:
      sandbox_config_filename = @"worker";
      break;
    case Sandbox::SANDBOX_TYPE_UTILITY:
      sandbox_config_filename = @"utility";
      break;
    case Sandbox::SANDBOX_TYPE_NACL_LOADER:
      // The Native Client loader is used for safeguarding the user's
      // untrusted code within Native Client.
      sandbox_config_filename = @"nacl_loader";
      break;
    case Sandbox::SANDBOX_TYPE_GPU:
      sandbox_config_filename = @"gpu";
      break;
    default:
      NOTREACHED();
      return nil;
  }

  // Read in the sandbox profile and the common prefix file.
  NSString* common_sandbox_prefix_path =
      [base::mac::MainAppBundle() pathForResource:@"common"
                                          ofType:@"sb"];
  NSString* common_sandbox_prefix_data =
      [NSString stringWithContentsOfFile:common_sandbox_prefix_path
                                encoding:NSUTF8StringEncoding
                                   error:NULL];

  if (!common_sandbox_prefix_data) {
    LOG(FATAL) << "Failed to find the sandbox profile on disk "
               << [common_sandbox_prefix_path fileSystemRepresentation];
    return nil;
  }

  NSString* sandbox_profile_path =
      [base::mac::MainAppBundle() pathForResource:sandbox_config_filename
                                          ofType:@"sb"];
  NSString* sandbox_data =
      [NSString stringWithContentsOfFile:sandbox_profile_path
                                 encoding:NSUTF8StringEncoding
                                    error:NULL];

  if (!sandbox_data) {
    LOG(FATAL) << "Failed to find the sandbox profile on disk "
               << [sandbox_profile_path fileSystemRepresentation];
    return nil;
  }

  // Prefix sandbox_data with common_sandbox_prefix_data.
  return [common_sandbox_prefix_data stringByAppendingString:sandbox_data];
}

// Retrieve OS X version, output parameters are self explanatory.
void GetOSVersion(bool* snow_leopard_or_higher) {
  int32 major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version,
                                               &minor_version,
                                               &bugfix_version);
  *snow_leopard_or_higher =
      (major_version > 10 || (major_version == 10 && minor_version >= 6));
}

// static
bool Sandbox::PostProcessSandboxProfile(
        NSString* sandbox_template,
        NSArray* comments_to_remove,
        SandboxVariableSubstitions& substitutions,
        std::string *final_sandbox_profile_str) {
  NSString* sandbox_data = [[sandbox_template copy] autorelease];

  // Remove comments, e.g. ;10.6_ONLY .
  for (NSString* to_remove in comments_to_remove) {
    sandbox_data = [sandbox_data stringByReplacingOccurrencesOfString:to_remove
                                                           withString:@""];
  }

  // Split string on "@" characters.
  std::vector<std::string> raw_sandbox_pieces;
  if (Tokenize([sandbox_data UTF8String], "@", &raw_sandbox_pieces) == 0) {
    LOG(FATAL) << "Bad Sandbox profile, should contain at least one token ("
               << [sandbox_data UTF8String]
               << ")";
    return false;
  }

  // Iterate over string pieces and substitute variables, escaping as necessary.
  size_t output_string_length = 0;
  std::vector<std::string> processed_sandbox_pieces(raw_sandbox_pieces.size());
  for (std::vector<std::string>::iterator it = raw_sandbox_pieces.begin();
       it != raw_sandbox_pieces.end();
       ++it) {
    std::string new_piece;
    SandboxVariableSubstitions::iterator replacement_it =
        substitutions.find(*it);
    if (replacement_it == substitutions.end()) {
      new_piece = *it;
    } else {
      // Found something to substitute.
      SandboxSubstring& replacement = replacement_it->second;
      switch (replacement.type()) {
        case SandboxSubstring::PLAIN:
          new_piece = replacement.value();
          break;

        case SandboxSubstring::LITERAL:
          QuotePlainString(replacement.value(), &new_piece);
          break;

        case SandboxSubstring::REGEX:
          QuoteStringForRegex(replacement.value(), &new_piece);
          break;
      }
    }
    output_string_length += new_piece.size();
    processed_sandbox_pieces.push_back(new_piece);
  }

  // Build final output string.
  final_sandbox_profile_str->reserve(output_string_length);

  for (std::vector<std::string>::iterator it = processed_sandbox_pieces.begin();
       it != processed_sandbox_pieces.end();
       ++it) {
    final_sandbox_profile_str->append(*it);
  }
  return true;
}


// Turns on the OS X sandbox for this process.

// static
bool Sandbox::EnableSandbox(SandboxProcessType sandbox_type,
                            const FilePath& allowed_dir) {
  // Sanity - currently only SANDBOX_TYPE_UTILITY supports a directory being
  // passed in.
  if (sandbox_type != SANDBOX_TYPE_UTILITY) {
    DCHECK(allowed_dir.empty())
        << "Only SANDBOX_TYPE_UTILITY allows a custom directory parameter.";
  }

  NSString* sandbox_data = LoadSandboxTemplate(sandbox_type);
  if (!sandbox_data) {
    return false;
  }

  SandboxVariableSubstitions substitutions;
  if (!allowed_dir.empty()) {
    // Add the sandbox commands necessary to access the given directory.
    // Note: this function must be called before PostProcessSandboxProfile()
    // since the string it inserts contains variables that need substitution.
    NSString* allowed_dir_sandbox_command =
        BuildAllowDirectoryAccessSandboxString(allowed_dir, &substitutions);

    if (allowed_dir_sandbox_command) {  // May be nil if function fails.
      sandbox_data = [sandbox_data
          stringByReplacingOccurrencesOfString:@";ENABLE_DIRECTORY_ACCESS"
                                    withString:allowed_dir_sandbox_command];
    }
  }

  NSMutableArray* tokens_to_remove = [NSMutableArray array];

  // Enable verbose logging if enabled on the command line. (See common.sb
  // for details).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(switches::kEnableSandboxLogging);;
  if (enable_logging) {
    [tokens_to_remove addObject:@";ENABLE_LOGGING"];
  }

  bool snow_leopard_or_higher;
  GetOSVersion(&snow_leopard_or_higher);

  // Without this, the sandbox will print a message to the system log every
  // time it denies a request.  This floods the console with useless spew. The
  // (with no-log) syntax is only supported on 10.6+
  if (snow_leopard_or_higher && !enable_logging) {
    substitutions["DISABLE_SANDBOX_DENIAL_LOGGING"] =
        SandboxSubstring("(with no-log)");
  } else {
    substitutions["DISABLE_SANDBOX_DENIAL_LOGGING"] = SandboxSubstring("");
  }

  // Splice the path of the user's home directory into the sandbox profile
  // (see renderer.sb for details).
  std::string home_dir = base::SysNSStringToUTF8(NSHomeDirectory());

  FilePath home_dir_canonical(home_dir);
  GetCanonicalSandboxPath(&home_dir_canonical);

  substitutions["USER_HOMEDIR_AS_LITERAL"] =
      SandboxSubstring(home_dir_canonical.value(),
          SandboxSubstring::LITERAL);

  if (snow_leopard_or_higher) {
    // 10.6-only Sandbox rules.
    [tokens_to_remove addObject:@";10.6_ONLY"];
  } else {
    // Sandbox rules only for versions before 10.6.
    [tokens_to_remove addObject:@";BEFORE_10.6"];
  }

  // All information needed to assemble the final profile has been collected.
  // Merge it all together.
  std::string final_sandbox_profile_str;
  if (!PostProcessSandboxProfile(sandbox_data, tokens_to_remove, substitutions,
                                 &final_sandbox_profile_str)) {
    return false;
  }

  // Initialize sandbox.
  char* error_buff = NULL;
  int error = sandbox_init(final_sandbox_profile_str.c_str(), 0, &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  LOG_IF(FATAL, !success) << "Failed to initialize sandbox: "
                          << error
                          << " "
                          << error_buff;
  sandbox_free_error(error_buff);
  return success;
}

// static
void Sandbox::GetCanonicalSandboxPath(FilePath* path) {
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
