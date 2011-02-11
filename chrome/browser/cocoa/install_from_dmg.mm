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

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/cocoa/authorization_util.h"
#include "chrome/browser/cocoa/scoped_authorizationref.h"
#import "chrome/browser/cocoa/keystone_glue.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace {

// Just like ScopedCFTypeRef but for io_object_t and subclasses.
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

// Potentially shows an authorization dialog to request authentication to
// copy.  If application_directory appears to be unwritable, attempts to
// obtain authorization, which may result in the display of the dialog.
// Returns NULL if authorization is not performed because it does not appear
// to be necessary because the user has permission to write to
// application_directory.  Returns NULL if authorization fails.
AuthorizationRef MaybeShowAuthorizationDialog(NSString* application_directory) {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  if ([file_manager isWritableFileAtPath:application_directory]) {
    return NULL;
  }

  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_INSTALL_FROM_DMG_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  return authorization_util::AuthorizationCreateToRunAsRoot(
      reinterpret_cast<CFStringRef>(prompt));
}

// Invokes the installer program at installer_path to copy source_path to
// target_path and perform any additional on-disk bookkeeping needed to be
// able to launch target_path properly.  If authorization_arg is non-NULL,
// function will assume ownership of it, will invoke the installer with that
// authorization reference, and will attempt Keystone ticket promotion.
bool InstallFromDiskImage(AuthorizationRef authorization_arg,
                          NSString* installer_path,
                          NSString* source_path,
                          NSString* target_path) {
  scoped_AuthorizationRef authorization(authorization_arg);
  authorization_arg = NULL;
  int exit_status;
  if (authorization) {
    const char* installer_path_c = [installer_path fileSystemRepresentation];
    const char* source_path_c = [source_path fileSystemRepresentation];
    const char* target_path_c = [target_path fileSystemRepresentation];
    const char* arguments[] = {source_path_c, target_path_c, NULL};

    OSStatus status = authorization_util::ExecuteWithPrivilegesAndWait(
        authorization,
        installer_path_c,
        kAuthorizationFlagDefaults,
        arguments,
        NULL,  // pipe
        &exit_status);
    if (status != errAuthorizationSuccess) {
      LOG(ERROR) << "AuthorizationExecuteWithPrivileges install: " << status;
      return false;
    }
  } else {
    NSArray* arguments = [NSArray arrayWithObjects:source_path,
                                                   target_path,
                                                   nil];

    NSTask* task;
    @try {
      task = [NSTask launchedTaskWithLaunchPath:installer_path
                                      arguments:arguments];
    } @catch(NSException* exception) {
      LOG(ERROR) << "+[NSTask launchedTaskWithLaunchPath:arguments:]: "
                 << [[exception description] UTF8String];
      return false;
    }

    [task waitUntilExit];
    exit_status = [task terminationStatus];
  }

  if (exit_status != 0) {
    LOG(ERROR) << "install.sh: exit status " << exit_status;
    return false;
  }

  if (authorization) {
    // As long as an AuthorizationRef is available, promote the Keystone
    // ticket.  Inform KeystoneGlue of the new path to use.
    KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
    [keystone_glue setAppPath:target_path];
    [keystone_glue promoteTicketWithAuthorization:authorization.release()
                                      synchronous:YES];
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
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

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
    VLOG(1) << "No application directory at "
            << [application_directory UTF8String];
    return false;
  }

  NSString* source_path = [[NSBundle mainBundle] bundlePath];
  NSString* application_name = [source_path lastPathComponent];
  NSString* target_path =
      [application_directory stringByAppendingPathComponent:application_name];

  if ([file_manager fileExistsAtPath:target_path]) {
    VLOG(1) << "Something already exists at " << [target_path UTF8String];
    return false;
  }

  NSString* installer_path =
      [base::mac::MainAppBundle() pathForResource:@"install" ofType:@"sh"];
  if (!installer_path) {
    VLOG(1) << "Could not locate install.sh";
    return false;
  }

  if (!ShouldInstallDialog()) {
    return false;
  }

  scoped_AuthorizationRef authorization(
      MaybeShowAuthorizationDialog(application_directory));
  // authorization will be NULL if it's deemed unnecessary or if
  // authentication fails.  In either case, try to install without privilege
  // escalation.

  if (!InstallFromDiskImage(authorization.release(),
                            installer_path,
                            source_path,
                            target_path) ||
      !LaunchInstalledApp(target_path)) {
    ShowErrorDialog();
    return false;
  }

  return true;
}
