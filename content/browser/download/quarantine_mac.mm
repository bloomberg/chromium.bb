// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/quarantine.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_restrictions.h"
#include "url/gurl.h"

namespace content {

namespace {

// As of Mac OS X 10.4 ("Tiger"), files can be tagged with metadata describing
// various attributes.  Metadata is integrated with the system's Spotlight
// feature and is searchable.  Ordinarily, metadata can only be set by
// Spotlight importers, which requires that the importer own the target file.
// However, there's an attribute intended to describe the origin of a
// file, that can store the source URL and referrer of a downloaded file.
// It's stored as a "com.apple.metadata:kMDItemWhereFroms" extended attribute,
// structured as a binary1-format plist containing a list of sources.  This
// attribute can only be populated by the downloader, not a Spotlight importer.
// Safari on 10.4 and later populates this attribute.
//
// With this metadata set, you can locate downloads by performing a Spotlight
// search for their source or referrer URLs, either from within the Spotlight
// UI or from the command line:
//     mdfind 'kMDItemWhereFroms == "http://releases.mozilla.org/*"'
//
// There is no documented API to set metadata on a file directly as of the
// 10.5 SDK.  The MDSetItemAttribute function does exist to perform this task,
// but it's undocumented.
bool AddOriginMetadataToFile(const base::FilePath& file,
                             const GURL& source,
                             const GURL& referrer) {
  base::ThreadRestrictions::AssertIOAllowed();
  // There's no declaration for MDItemSetAttribute in any known public SDK.
  // It exists in the 10.4 and 10.5 runtimes.  To play it safe, do the lookup
  // at runtime instead of declaring it ourselves and linking against what's
  // provided.  This has two benefits:
  //  - If Apple relents and declares the function in a future SDK (it's
  //    happened before), our build won't break.
  //  - If Apple removes or renames the function in a future runtime, the
  //    loader won't refuse to let the application launch.  Instead, we'll
  //    silently fail to set any metadata.
  typedef OSStatus (*MDItemSetAttribute_type)(MDItemRef, CFStringRef,
                                              CFTypeRef);
  static MDItemSetAttribute_type md_item_set_attribute_func = NULL;

  static bool did_symbol_lookup = false;
  if (!did_symbol_lookup) {
    did_symbol_lookup = true;
    CFBundleRef metadata_bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Metadata"));
    if (!metadata_bundle)
      return false;

    md_item_set_attribute_func =
        (MDItemSetAttribute_type)CFBundleGetFunctionPointerForName(
            metadata_bundle, CFSTR("MDItemSetAttribute"));
  }
  if (!md_item_set_attribute_func)
    return false;

  NSString* file_path = [NSString stringWithUTF8String:file.value().c_str()];
  if (!file_path)
    return false;

  base::ScopedCFTypeRef<MDItemRef> md_item(
      MDItemCreate(NULL, base::mac::NSToCFCast(file_path)));
  if (!md_item)
    return false;

  // We won't put any more than 2 items into the attribute.
  NSMutableArray* list = [NSMutableArray arrayWithCapacity:2];

  // Follow Safari's lead: the first item in the list is the source URL of
  // the downloaded file. If the referrer is known, store that, too.
  NSString* origin_url = [NSString stringWithUTF8String:source.spec().c_str()];
  if (origin_url)
    [list addObject:origin_url];
  NSString* referrer_url =
      [NSString stringWithUTF8String:referrer.spec().c_str()];
  if (referrer_url)
    [list addObject:referrer_url];

  md_item_set_attribute_func(md_item, kMDItemWhereFroms,
                             base::mac::NSToCFCast(list));
  return true;
}

// Adds quarantine metadata to the file, assuming it has already been
// quarantined by the OS.
// |source| should be the source URL for the download, and |referrer| should be
// the URL the user initiated the download from.

// The OS will automatically quarantine files due to the
// LSFileQuarantineEnabled entry in our Info.plist, but it knows relatively
// little about the files. We add more information about the download to
// improve the UI shown by the OS when the users tries to open the file.
bool AddQuarantineMetadataToFile(const base::FilePath& file,
                                 const GURL& source,
                                 const GURL& referrer) {
  base::ThreadRestrictions::AssertIOAllowed();
  FSRef file_ref;
  if (!base::mac::FSRefFromPath(file.value(), &file_ref))
    return false;

  NSMutableDictionary* quarantine_properties = nil;
  CFTypeRef quarantine_properties_base = NULL;
  if (LSCopyItemAttribute(&file_ref, kLSRolesAll, kLSItemQuarantineProperties,
                          &quarantine_properties_base) == noErr) {
    if (CFGetTypeID(quarantine_properties_base) == CFDictionaryGetTypeID()) {
      // Quarantine properties will already exist if LSFileQuarantineEnabled
      // is on and the file doesn't match an exclusion.
      quarantine_properties =
          [[(NSDictionary*)quarantine_properties_base mutableCopy] autorelease];
    } else {
      LOG(WARNING) << "kLSItemQuarantineProperties is not a dictionary on file "
                   << file.value();
    }
    CFRelease(quarantine_properties_base);
  }

  if (!quarantine_properties) {
    // If there are no quarantine properties, then the file isn't quarantined
    // (e.g., because the user has set up exclusions for certain file types).
    // We don't want to add any metadata, because that will cause the file to
    // be quarantined against the user's wishes.
    return true;
  }

  // kLSQuarantineAgentNameKey, kLSQuarantineAgentBundleIdentifierKey, and
  // kLSQuarantineTimeStampKey are set for us (see LSQuarantine.h), so we only
  // need to set the values that the OS can't infer.

  if (![quarantine_properties valueForKey:(NSString*)kLSQuarantineTypeKey]) {
    CFStringRef type = source.SchemeIsHTTPOrHTTPS()
                           ? kLSQuarantineTypeWebDownload
                           : kLSQuarantineTypeOtherDownload;
    [quarantine_properties setValue:(NSString*)type
                             forKey:(NSString*)kLSQuarantineTypeKey];
  }

  if (![quarantine_properties
          valueForKey:(NSString*)kLSQuarantineOriginURLKey] &&
      referrer.is_valid()) {
    NSString* referrer_url =
        [NSString stringWithUTF8String:referrer.spec().c_str()];
    [quarantine_properties setValue:referrer_url
                             forKey:(NSString*)kLSQuarantineOriginURLKey];
  }

  if (![quarantine_properties valueForKey:(NSString*)kLSQuarantineDataURLKey] &&
      source.is_valid()) {
    NSString* origin_url =
        [NSString stringWithUTF8String:source.spec().c_str()];
    [quarantine_properties setValue:origin_url
                             forKey:(NSString*)kLSQuarantineDataURLKey];
  }

  OSStatus os_error =
      LSSetItemAttribute(&file_ref, kLSRolesAll, kLSItemQuarantineProperties,
                         quarantine_properties);
  if (os_error != noErr) {
    OSSTATUS_LOG(WARNING, os_error)
        << "Unable to set quarantine attributes on file " << file.value();
    return false;
  }
  return true;
}

}  // namespace

QuarantineFileResult QuarantineFile(const base::FilePath& file,
                                    const GURL& source_url,
                                    const GURL& referrer_url,
                                    const std::string& client_guid) {
  bool quarantine_succeeded =
      AddQuarantineMetadataToFile(file, source_url, referrer_url);
  bool origin_succeeded =
      AddOriginMetadataToFile(file, source_url, referrer_url);
  return quarantine_succeeded && origin_succeeded
             ? QuarantineFileResult::OK
             : QuarantineFileResult::ANNOTATION_FAILED;
}

}  // namespace content
