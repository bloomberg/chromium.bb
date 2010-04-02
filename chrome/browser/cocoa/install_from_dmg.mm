// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/install_from_dmg.h"

#include <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/mount.h>

#import "app/l10n_util_mac.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/sys_info.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace {

// Just like scoped_cftyperef from base/scoped_cftyperef.h, but for
// io_object_t and subclasses.
template<typename IOT>
class scoped_ioobject {
 public:
  typedef IOT element_type;

  explicit scoped_ioobject(IOT object = NULL)
      : object_(object) {
  }

  ~scoped_ioobject() {
    if (object_)
      IOObjectRelease(object_);
  }

  void reset(IOT object = NULL) {
    if (object_)
      IOObjectRelease(object_);
    object_ = object;
  }

  bool operator==(IOT that) const {
    return object_ == that;
  }

  bool operator!=(IOT that) const {
    return object_ != that;
  }

  operator IOT() const {
    return object_;
  }

  IOT get() const {
    return object_;
  }

  void swap(scoped_ioobject& that) {
    IOT temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  IOT release() {
    IOT temp = object_;
    object_ = NULL;
    return temp;
  }

 private:
  IOT object_;

  DISALLOW_COPY_AND_ASSIGN(scoped_ioobject);
};

// Returns true if |path| is located on a read-only filesystem of a disk
// image.  Returns false if not, or in the event of an error.
bool IsPathOnReadOnlyDiskImage(const char path[]) {
  struct statfs statfs_buf;
  if (statfs(path, &statfs_buf) != 0) {
    PLOG(ERROR) << "statfs " << path;
    return false;
  }

  if (!(statfs_buf.f_flags & MNT_RDONLY)) {
    // Not on a read-only filesystem.
    return false;
  }

  const char dev_root[] = "/dev/";
  const int dev_root_length = arraysize(dev_root) - 1;
  if (strncmp(statfs_buf.f_mntfromname, dev_root, dev_root_length) != 0) {
    // Not rooted at dev_root, no BSD name to search on.
    return false;
  }

  // BSD names in IOKit don't include dev_root.
  const char* bsd_device_name = statfs_buf.f_mntfromname + dev_root_length;

  const mach_port_t master_port = kIOMasterPortDefault;

  // IOBSDNameMatching gives ownership of match_dict to the caller, but
  // IOServiceGetMatchingServices will assume that reference.
  CFMutableDictionaryRef match_dict = IOBSDNameMatching(master_port,
                                                        0,
                                                        bsd_device_name);
  if (!match_dict) {
    LOG(ERROR) << "IOBSDNameMatching " << bsd_device_name;
    return false;
  }

  io_iterator_t iterator_ref;
  kern_return_t kr = IOServiceGetMatchingServices(master_port,
                                                  match_dict,
                                                  &iterator_ref);
  if (kr != KERN_SUCCESS) {
    LOG(ERROR) << "IOServiceGetMatchingServices " << bsd_device_name
               << ": kernel error " << kr;
    return false;
  }
  scoped_ioobject<io_iterator_t> iterator(iterator_ref);
  iterator_ref = NULL;

  // There needs to be exactly one matching service.
  scoped_ioobject<io_service_t> filesystem_service(IOIteratorNext(iterator));
  if (!filesystem_service) {
    LOG(ERROR) << "IOIteratorNext " << bsd_device_name << ": no service";
    return false;
  }
  scoped_ioobject<io_service_t> unexpected_service(IOIteratorNext(iterator));
  if (unexpected_service) {
    LOG(ERROR) << "IOIteratorNext " << bsd_device_name << ": too many services";
    return false;
  }

  iterator.reset();

  const char disk_image_class[] = "IOHDIXController";

  // This is highly unlikely.  The filesystem service is expected to be of
  // class IOMedia.  Since the filesystem service's entire ancestor chain
  // will be checked, though, check the filesystem service's class itself.
  if (IOObjectConformsTo(filesystem_service, disk_image_class)) {
    return true;
  }

  kr = IORegistryEntryCreateIterator(filesystem_service,
                                     kIOServicePlane,
                                     kIORegistryIterateRecursively |
                                         kIORegistryIterateParents,
                                     &iterator_ref);
  if (kr != KERN_SUCCESS) {
    LOG(ERROR) << "IORegistryEntryCreateIterator " << bsd_device_name
               << ": kernel error " << kr;
    return false;
  }
  iterator.reset(iterator_ref);
  iterator_ref = NULL;

  // Look at each of the filesystem service's ancestor services, beginning
  // with the parent, iterating all the way up to the device tree's root.  If
  // any ancestor service matches the class used for disk images, the
  // filesystem resides on a disk image.
  for(scoped_ioobject<io_service_t> ancestor_service(IOIteratorNext(iterator));
      ancestor_service;
      ancestor_service.reset(IOIteratorNext(iterator))) {
    if (IOObjectConformsTo(ancestor_service, disk_image_class)) {
      return true;
    }
  }

  // The filesystem does not reside on a disk image.
  return false;
}

// Returns true if the application is located on a read-only filesystem of a
// disk image.  Returns false if not, or in the event of an error.
bool IsAppRunningFromReadOnlyDiskImage() {
  return IsPathOnReadOnlyDiskImage(
      [[[NSBundle mainBundle] bundlePath] fileSystemRepresentation]);
}

// Shows a dialog asking the user whether or not to install from the disk
// image.  Returns true if the user approves installation.
bool ShouldInstallDialog() {
  NSString* title = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_TITLE, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_PROMPT, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* yes = l10n_util::GetNSStringWithFixup(IDS_INSTALL_FROM_DMG_YES);
  NSString* no = l10n_util::GetNSStringWithFixup(IDS_INSTALL_FROM_DMG_NO);

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];

  [alert setAlertStyle:NSInformationalAlertStyle];
  [alert setMessageText:title];
  [alert setInformativeText:prompt];
  [alert addButtonWithTitle:yes];
  NSButton* cancel_button = [alert addButtonWithTitle:no];
  [cancel_button setKeyEquivalent:@"\e"];

  NSInteger result = [alert runModal];

  return result == NSAlertFirstButtonReturn;
}

// Copies source_path to target_path and performs any additional on-disk
// bookkeeping needed to be able to launch target_path properly.
bool InstallFromDiskImage(NSString* source_path, NSString* target_path) {
  NSFileManager* file_manager = [NSFileManager defaultManager];

  // For the purposes of this copy, the file manager's delegate shouldn't be
  // consulted at all.  Clear the delegate and restore it after the copy is
  // done.
  id file_manager_delegate = [file_manager delegate];
  [file_manager setDelegate:nil];

  NSError* copy_error;
  bool copy_result = [file_manager copyItemAtPath:source_path
                                           toPath:target_path
                                            error:&copy_error];

  [file_manager setDelegate:file_manager_delegate];

  if (!copy_result) {
    LOG(ERROR) << "-[NSFileManager copyItemAtPath:toPath:error:]: "
               << [[copy_error description] UTF8String];
    return false;
  }

  // Since the application performed the copy, and the application has the
  // quarantine bit (LSFileQuarantineEnabled) set, the installed copy will
  // be quarantined.  That's bad, because it will cause the quarantine dialog
  // to be displayed, possibly after a long delay, when the application is
  // relaunched.  Use xattr to drop the quarantine attribute.
  //
  // There are three reasons not to use MDItemRemoveAttribute directly:
  // 1. MDItemRemoveAttribute is a private API.
  // 2. The operation needs to be recursive, and writing a bunch of code to
  //    handle the recursion just to call a private API is annoying.
  // 3. All of this stuff will likely move into a shell script anyway, and
  //    the shell script will have no choice but to use xattr.

  int32 os_major, os_minor, os_patch;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major, &os_minor, &os_patch);

  const NSString* xattr_path = @"/usr/bin/xattr";
  const NSString* quarantine_attribute = @"com.apple.quarantine";
  NSString* launch_path;
  NSArray* arguments;

  if (os_major > 10 || (os_major == 10 && os_minor >= 6)) {
    // On 10.6, xattr supports -r for recursive operation.
    launch_path = xattr_path;
    arguments = [NSArray arrayWithObjects:@"-r",
                                          @"-d",
                                          quarantine_attribute,
                                          target_path,
                                          nil];
  } else {
    // On earlier systems, xattr doesn't support -r, so run xattr via find.
    launch_path = @"/usr/bin/find";
    arguments = [NSArray arrayWithObjects:target_path,
                                          @"-exec",
                                          xattr_path,
                                          @"-d",
                                          quarantine_attribute,
                                          @"{}",
                                          @"+",
                                          nil];
  }

  NSTask* task;
  @try {
    task = [NSTask launchedTaskWithLaunchPath:launch_path
                                    arguments:arguments];
  } @catch(NSException* exception) {
    LOG(ERROR) << "+[NSTask launchedTaskWithLaunchPath:arguments:]: "
               << [[exception description] UTF8String];
    return false;
  }

  [task waitUntilExit];
  int status = [task terminationStatus];
  if (status != 0) {
    LOG(ERROR) << "/usr/bin/xattr: status " << status;
    return false;
  }

  return true;
}

// Launches the application at app_path.  The arguments passed to app_path
// will be the same as the arguments used to invoke this process, except any
// arguments beginning with -psn_ will be stripped.
bool LaunchInstalledApp(NSString* app_path) {
  const UInt8* app_path_c =
      reinterpret_cast<const UInt8*>([app_path fileSystemRepresentation]);
  FSRef app_fsref;
  OSStatus err = FSPathMakeRef(app_path_c, &app_fsref, NULL);
  if (err != noErr) {
    LOG(ERROR) << "FSPathMakeRef: " << err;
    return false;
  }

  // Use an empty dictionary for the environment.
  NSDictionary* environment = [NSDictionary dictionary];

  const std::vector<std::string>& argv =
      CommandLine::ForCurrentProcess()->argv();
  NSMutableArray* arguments =
      [NSMutableArray arrayWithCapacity:argv.size() - 1];
  // Start at argv[1].  LSOpenApplication adds its own argv[0] as the path of
  // the launched executable.
  for (size_t index = 1; index < argv.size(); ++index) {
    std::string argument = argv[index];
    const char psn_flag[] = "-psn_";
    const int psn_flag_length = arraysize(psn_flag) - 1;
    if (argument.compare(0, psn_flag_length, psn_flag) != 0) {
      // Strip any -psn_ arguments, as they apply to a specific process.
      [arguments addObject:[NSString stringWithUTF8String:argument.c_str()]];
    }
  }

  struct LSApplicationParameters parameters = {0};
  parameters.flags = kLSLaunchDefaults;
  parameters.application = &app_fsref;
  parameters.environment = reinterpret_cast<CFDictionaryRef>(environment);
  parameters.argv = reinterpret_cast<CFArrayRef>(arguments);

  err = LSOpenApplication(&parameters, NULL);
  if (err != noErr) {
    LOG(ERROR) << "LSOpenApplication: " << err;
    return false;
  }

  return true;
}

void ShowErrorDialog() {
  NSString* title = l10n_util::GetNSStringWithFixup(
      IDS_INSTALL_FROM_DMG_ERROR_TITLE);
  NSString* error = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_ERROR, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* ok = l10n_util::GetNSStringWithFixup(IDS_OK);

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];

  [alert setAlertStyle:NSWarningAlertStyle];
  [alert setMessageText:title];
  [alert setInformativeText:error];
  [alert addButtonWithTitle:ok];

  [alert runModal];
}

}  // namespace

bool MaybeInstallFromDiskImage() {
  base::ScopedNSAutoreleasePool autorelease_pool;

  if (!IsAppRunningFromReadOnlyDiskImage()) {
    return false;
  }

  NSArray* application_directories =
      NSSearchPathForDirectoriesInDomains(NSApplicationDirectory,
                                          NSLocalDomainMask,
                                          YES);
  if ([application_directories count] == 0) {
    LOG(ERROR) << "NSSearchPathForDirectoriesInDomains: "
               << "no local application directories";
    return false;
  }
  NSString* application_directory = [application_directories objectAtIndex:0];

  NSFileManager* file_manager = [NSFileManager defaultManager];

  BOOL is_directory;
  if (![file_manager fileExistsAtPath:application_directory
                          isDirectory:&is_directory] ||
      !is_directory) {
    LOG(INFO) << "No application directory at "
              << [application_directory UTF8String];
    return false;
  }

  // TODO(mark): When this happens, prompt for authentication.
  if (![file_manager isWritableFileAtPath:application_directory]) {
    LOG(INFO) << "Non-writable application directory at "
              << [application_directory UTF8String];
    return false;
  }

  NSString* source_path = [[NSBundle mainBundle] bundlePath];
  NSString* application_name = [source_path lastPathComponent];
  NSString* target_path =
      [application_directory stringByAppendingPathComponent:application_name];

  if ([file_manager fileExistsAtPath:target_path]) {
    LOG(INFO) << "Something already exists at " << [target_path UTF8String];
    return false;
  }

  if (!ShouldInstallDialog()) {
    return false;
  }

  if (!InstallFromDiskImage(source_path, target_path) ||
      !LaunchInstalledApp(target_path)) {
    ShowErrorDialog();
    return false;
  }

  return true;
}
