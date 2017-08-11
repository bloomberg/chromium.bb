// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include <CoreFoundation/CFTimeZone.h>
#include <signal.h>
#include <sys/param.h>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "content/grit/content_resources.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/gpu/vt_video_decode_accelerator_mac.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/base/layout.h"
#include "ui/gl/init/gl_factory.h"

namespace content {
namespace {

// Is the sandbox currently active.
bool gSandboxIsActive = false;

struct SandboxTypeToResourceIDMapping {
  SandboxType sandbox_type;
  int sandbox_profile_resource_id;
};

// Mapping from sandbox process types to resource IDs containing the sandbox
// profile for all process types known to content.
// TODO(tsepez): Implement profile for SANDBOX_TYPE_NETWORK.
SandboxTypeToResourceIDMapping kDefaultSandboxTypeToResourceIDMapping[] = {
    {SANDBOX_TYPE_NO_SANDBOX, -1},
    {SANDBOX_TYPE_RENDERER, IDR_RENDERER_SANDBOX_PROFILE},
    {SANDBOX_TYPE_UTILITY, IDR_UTILITY_SANDBOX_PROFILE},
    {SANDBOX_TYPE_GPU, IDR_GPU_SANDBOX_PROFILE},
    {SANDBOX_TYPE_PPAPI, IDR_PPAPI_SANDBOX_PROFILE},
    {SANDBOX_TYPE_NETWORK, -1},
};

static_assert(arraysize(kDefaultSandboxTypeToResourceIDMapping) == \
              size_t(SANDBOX_TYPE_AFTER_LAST_TYPE), \
              "sandbox type to resource id mapping incorrect");

}  // namespace

// Static variable declarations.
const char* Sandbox::kSandboxBrowserPID = "BROWSER_PID";
const char* Sandbox::kSandboxBundlePath = "BUNDLE_PATH";
const char* Sandbox::kSandboxChromeBundleId = "BUNDLE_ID";
const char* Sandbox::kSandboxComponentPath = "COMPONENT_PATH";
const char* Sandbox::kSandboxDisableDenialLogging =
    "DISABLE_SANDBOX_DENIAL_LOGGING";
const char* Sandbox::kSandboxEnableLogging = "ENABLE_LOGGING";
const char* Sandbox::kSandboxHomedirAsLiteral = "USER_HOMEDIR_AS_LITERAL";
const char* Sandbox::kSandboxLoggingPathAsLiteral = "LOG_FILE_PATH";
const char* Sandbox::kSandboxOSVersion = "OS_VERSION";
const char* Sandbox::kSandboxPermittedDir = "PERMITTED_DIR";

const char* Sandbox::kSandboxElCapOrLater = "ELCAP_OR_LATER";
const char* Sandbox::kSandboxMacOS1013 = "MACOS_1013";

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6.0

// static
void Sandbox::SandboxWarmup(int sandbox_type) {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  { // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
    base::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

    // Allocate a 1x1 image.
    char data[4];
    base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
        data,
        1,
        1,
        8,
        1 * 4,
        rgb_colorspace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

    // Load in the color profiles we'll need (as a side effect).
    ignore_result(base::mac::GetSRGBColorSpace());
    ignore_result(base::mac::GetSystemColorSpace());

    // CGColorSpaceCreateSystemDefaultCMYK - 10.6
    base::ScopedCFTypeRef<CGColorSpaceRef> cmyk_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK));
  }

  { // localtime() - 10.5.6
    time_t tv = {0};
    localtime(&tv);
  }

  { // Gestalt() tries to read /System/Library/CoreServices/SystemVersion.plist
    // on 10.5.6
    int32_t tmp;
    base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
  }

  {  // CGImageSourceGetStatus() - 10.6
     // Create a png with just enough data to get everything warmed up...
    char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    NSData* data = [NSData dataWithBytes:png_header
                                  length:arraysize(png_header)];
    base::ScopedCFTypeRef<CGImageSourceRef> img(
        CGImageSourceCreateWithData((CFDataRef)data, NULL));
    CGImageSourceGetStatus(img);
  }

  {
    // Allow access to /dev/urandom.
    base::GetUrandomFD();
  }

  { // IOSurfaceLookup() - 10.7
    // Needed by zero-copy texture update framework - crbug.com/323338
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceLookup(0));
  }

  // Process-type dependent warm-up.
  if (sandbox_type == SANDBOX_TYPE_UTILITY) {
    // CFTimeZoneCopyZone() tries to read /etc and /private/etc/localtime - 10.8
    // Needed by Media Galleries API Picasa - crbug.com/151701
    CFTimeZoneCopySystem();
  }

  if (sandbox_type == SANDBOX_TYPE_GPU) {
    // Preload either the desktop GL or the osmesa so, depending on the
    // --use-gl flag.
    gl::init::InitializeGLOneOff();

    // Preload VideoToolbox.
    media::InitializeVideoToolbox();
  }

  if (sandbox_type == SANDBOX_TYPE_PPAPI) {
    // Preload AppKit color spaces used for Flash/ppapi. http://crbug.com/348304
    NSColor* color = [NSColor controlTextColor];
    [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  }
}

// Load the appropriate template for the given sandbox type.
// Returns the template as a string or an empty string on error.
std::string LoadSandboxTemplate(int sandbox_type) {
  // We use a custom sandbox definition to lock things down as tightly as
  // possible.
  int sandbox_profile_resource_id = -1;

  // Find resource id for sandbox profile to use for the specific sandbox type.
  for (size_t i = 0;
       i < arraysize(kDefaultSandboxTypeToResourceIDMapping);
       ++i) {
    if (kDefaultSandboxTypeToResourceIDMapping[i].sandbox_type ==
        sandbox_type) {
      sandbox_profile_resource_id =
          kDefaultSandboxTypeToResourceIDMapping[i].sandbox_profile_resource_id;
      break;
    }
  }
  if (sandbox_profile_resource_id == -1) {
    // Check if the embedder knows about this sandbox process type.
    bool sandbox_type_found =
        GetContentClient()->GetSandboxProfileForSandboxType(
            sandbox_type, &sandbox_profile_resource_id);
    CHECK(sandbox_type_found) << "Unknown sandbox type " << sandbox_type;
  }

  base::StringPiece sandbox_definition =
      GetContentClient()->GetDataResource(
          sandbox_profile_resource_id, ui::SCALE_FACTOR_NONE);
  if (sandbox_definition.empty()) {
    LOG(FATAL) << "Failed to load the sandbox profile (resource id "
               << sandbox_profile_resource_id << ")";
    return std::string();
  }

  base::StringPiece common_sandbox_definition =
      GetContentClient()->GetDataResource(
          IDR_COMMON_SANDBOX_PROFILE, ui::SCALE_FACTOR_NONE);
  if (common_sandbox_definition.empty()) {
    LOG(FATAL) << "Failed to load the common sandbox profile";
    return std::string();
  }

  // Prefix sandbox_data with common_sandbox_prefix_data.
  std::string sandbox_profile = common_sandbox_definition.as_string();
  sandbox_definition.AppendToString(&sandbox_profile);
  return sandbox_profile;
}

// Turns on the OS X sandbox for this process.

// static
bool Sandbox::EnableSandbox(int sandbox_type,
                            const base::FilePath& allowed_dir) {
  // Sanity - currently only SANDBOX_TYPE_UTILITY supports a directory being
  // passed in.
  if (sandbox_type < SANDBOX_TYPE_AFTER_LAST_TYPE &&
      sandbox_type != SANDBOX_TYPE_UTILITY) {
    DCHECK(allowed_dir.empty())
        << "Only SANDBOX_TYPE_UTILITY allows a custom directory parameter.";
  }

  std::string sandbox_data = LoadSandboxTemplate(sandbox_type);
  if (sandbox_data.empty()) {
    return false;
  }

  sandbox::SandboxCompiler compiler(sandbox_data);

  if (!allowed_dir.empty()) {
    // Add the sandbox parameters necessary to access the given directory.
    base::FilePath allowed_dir_canonical = GetCanonicalSandboxPath(allowed_dir);
    if (!compiler.InsertStringParam(kSandboxPermittedDir,
                                    allowed_dir_canonical.value()))
      return false;
  }

  // Enable verbose logging if enabled on the command line. (See common.sb
  // for details).
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(switches::kEnableSandboxLogging);;
  if (!compiler.InsertBooleanParam(kSandboxEnableLogging, enable_logging))
    return false;

  // Without this, the sandbox will print a message to the system log every
  // time it denies a request.  This floods the console with useless spew.
  if (!compiler.InsertBooleanParam(kSandboxDisableDenialLogging,
                                   !enable_logging))
    return false;

  // Splice the path of the user's home directory into the sandbox profile
  // (see renderer.sb for details).
  std::string home_dir = [NSHomeDirectory() fileSystemRepresentation];

  base::FilePath home_dir_canonical =
      GetCanonicalSandboxPath(base::FilePath(home_dir));

  if (!compiler.InsertStringParam(kSandboxHomedirAsLiteral,
                                  home_dir_canonical.value()))
    return false;

  bool elcap_or_later = base::mac::IsAtLeastOS10_11();
  if (!compiler.InsertBooleanParam(kSandboxElCapOrLater, elcap_or_later))
    return false;

  bool macos_1013 = base::mac::IsOS10_13();
  if (!compiler.InsertBooleanParam(kSandboxMacOS1013, macos_1013))
    return false;

  // Initialize sandbox.
  std::string error_str;
  bool success = compiler.CompileAndApplyProfile(&error_str);
  DLOG_IF(FATAL, !success) << "Failed to initialize sandbox: " << error_str;
  gSandboxIsActive = success;
  return success;
}

// static
bool Sandbox::SandboxIsCurrentlyActive() {
  return gSandboxIsActive;
}

// static
base::FilePath Sandbox::GetCanonicalSandboxPath(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(FATAL) << "GetCanonicalSandboxPath() failed for: "
                 << path.value();
    return path;
  }

  base::FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd.get(), F_GETPATH, canonical_path)) != 0) {
    DPLOG(FATAL) << "GetCanonicalSandboxPath() failed for: "
                 << path.value();
    return path;
  }

  return base::FilePath(canonical_path);
}

}  // namespace content
