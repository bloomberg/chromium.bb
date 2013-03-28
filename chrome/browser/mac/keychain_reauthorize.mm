// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/keychain_reauthorize.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/mac/security_wrappers.h"

namespace chrome {

namespace {

// Returns the requirement string embedded within a SecTrustedApplicationRef,
// or an empty string on error.
std::string RequirementStringForApplication(
    SecTrustedApplicationRef application);

// Returns the set of requirement strings that ought to be reauthorized. In a
// bundled application, the requirement string from |application| will also be
// added to the hard-coded list. This allows an at-launch reauthorization to
// re-reauthorize anything done by a previous at-update reauthorization.
// Although items reauthorized during the at-update step will work properly in
// every way, they contain a reference to the missing reauthorization stub
// executable from the disk image in the Keychain, resulting in no icon and
// a weird name like "com.google" (non-Canary) or "com.google.Chrome"
// (Canary). Because reauthorization is controlled by a preference that limits
// it to a single successful run at update and a single successful run at
// launch, protection already exists against perpetually reauthorizing items.
// This addition exists simply to make the Keychain Access UI match
// expectations.
std::vector<std::string> GetRequirementMatches(
    SecTrustedApplicationRef application);

// Reauthorizes an ACL by examining all of the applications it names, and upon
// finding any whose requirement matches any element of requirement_matches,
// replaces them with this_application. At most one instance of
// this_application will be added to the ACL. Subsequent applications whose
// requirement matches any element of requirement_matches will be removed from
// the ACL. Only the ACL is changed, nothing is written to disk. Returns true
// if any reauthorization is performed and thus acl is modified, and false
// otherwise.
bool ReauthorizeACL(
    SecACLRef acl,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application);

// Reauthorizes a list of ACLs by calling ReauthorizeACL for each ACL in the
// list. Only the ACL list is changed, nothing is written to disk. Returns
// true if ReauthorizeTrue returns true for any ACL in acl_list, indicating
// that at least one ACL in acl_list was modified and thus at least one child
// child of acl_list was reauthorized.
bool ReauthorizeACLList(
    CFArrayRef acl_list,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application);

// Reauthorizes a SecKeychainItemRef by calling ReauthorizeACLList to perform
// reauthorization on all ACLs that it contains. Nothing is written to disk.
// If any reauthorization was performed, returns a CrSKeychainItemAndAccess
// object containing the item and its access information. Otherwise, returns
// NULL.
CrSKeychainItemAndAccess* KCItemToKCItemAndReauthorizedAccess(
    SecKeychainItemRef item,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application);

// Reauthorizes multiple Keychain items by calling
// KCItemToKCItemAndReauthorizedAccess for each item returned by a Keychain
// search. Nothing is written to disk. Reauthorized items are returned.
std::vector<CrSKeychainItemAndAccess> KCSearchToKCItemsAndReauthorizedAccesses(
    SecKeychainSearchRef search,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application);

// Given a SecKeychainAttributeList, strips out any zero-length attributes and
// returns a vector containing the remaining attributes.
std::vector<SecKeychainAttribute> KCAttributesWithoutZeroLength(
    SecKeychainAttributeList* old_attribute_list);

// Given a CrSKeychainItemAndAccess that has had its access field
// reauthorized, places the reauthorized form into the Keychain by deleting
// the old item and replacing it with a new one whose access policy matches
// the reauthorized form. The new item is written to disk and becomes part of
// the Keychain, replacing what had been there previously.
void WriteKCItemAndReauthorizedAccess(
    const CrSKeychainItemAndAccess& item_and_reauthorized_access);

// Given a vector of CrSKeychainItemAndAccess objects, places the reauthorized
// forms of all of them into the Keychain by calling
// WriteKCItemAndReauthorizedAccess for each. The new items are written to
// disk and become part of the Keychain, replacing what had been there
// previously.
void WriteKCItemsAndReauthorizedAccesses(
    const std::vector<CrSKeychainItemAndAccess>&
        items_and_reauthorized_accesses);

}  // namespace

void KeychainReauthorize() {
  ScopedSecKeychainSetUserInteractionAllowed user_interaction_allowed(FALSE);

  // Apple's documentation (Keychain Services Reference, Constants/Mac OS X
  // Keychain Services API Constants/Keychain Item Class Constants) says to
  // use CSSM_DL_DB_RECORD_ALL_KEYS, but that doesn't work.
  // CSSM_DL_DB_RECORD_ANY (as used by SecurityTool's keychain-dump) does
  // work.
  base::mac::ScopedCFTypeRef<SecKeychainSearchRef> search(
    CrSKeychainSearchCreateFromAttributes(NULL,
                                          CSSM_DL_DB_RECORD_ANY,
                                          NULL));

  base::mac::ScopedCFTypeRef<SecTrustedApplicationRef> this_application(
      CrSTrustedApplicationCreateFromPath(NULL));

  std::vector<std::string> requirement_matches =
      GetRequirementMatches(this_application);

  std::vector<CrSKeychainItemAndAccess> items_and_reauthorized_accesses =
      KCSearchToKCItemsAndReauthorizedAccesses(search,
                                               requirement_matches,
                                               this_application);

  WriteKCItemsAndReauthorizedAccesses(items_and_reauthorized_accesses);
}

void KeychainReauthorizeIfNeeded(NSString* pref_key, int max_tries) {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  int pref_value = [user_defaults integerForKey:pref_key];

  if (pref_value < max_tries) {
    if (pref_value > 0) {
      // Logs the number of previous tries that didn't complete.
      if (base::mac::AmIBundled()) {
        UMA_HISTOGRAM_COUNTS("OSX.KeychainReauthorizeIfNeeded", pref_value);
      } else {
        UMA_HISTOGRAM_COUNTS("OSX.KeychainReauthorizeIfNeededAtUpdate",
                             pref_value);
      }
    }

    ++pref_value;
    [user_defaults setInteger:pref_value forKey:pref_key];
    [user_defaults synchronize];

    KeychainReauthorize();

    [user_defaults setInteger:max_tries forKey:pref_key];
    NSString* success_pref_key = [pref_key stringByAppendingString:@"Success"];
    [user_defaults setBool:YES forKey:success_pref_key];
    [user_defaults synchronize];

    // Logs the try number (1, 2) that succeeded.
    if (base::mac::AmIBundled()) {
      UMA_HISTOGRAM_COUNTS("OSX.KeychainReauthorizeIfNeededSuccess",
                           pref_value);
    } else {
      UMA_HISTOGRAM_COUNTS("OSX.KeychainReauthorizeIfNeededAtUpdateSuccess",
                           pref_value);
    }
  }
}

namespace {

std::string RequirementStringForApplication(
    SecTrustedApplicationRef application) {
  base::mac::ScopedCFTypeRef<SecRequirementRef> requirement(
      CrSTrustedApplicationCopyRequirement(application));
  base::mac::ScopedCFTypeRef<CFStringRef> requirement_string_cf(
      CrSRequirementCopyString(requirement, kSecCSDefaultFlags));
  if (!requirement_string_cf) {
    return std::string();
  }

  std::string requirement_string =
      base::SysCFStringRefToUTF8(requirement_string_cf);

  return requirement_string;
}

std::vector<std::string> GetRequirementMatches(
      SecTrustedApplicationRef application) {
  // See the designated requirement for a signed released build:
  // codesign -d -r- "Google Chrome.app"
  //
  // Export the certificates from a signed released build:
  // codesign -v --extract-certificates=/tmp/cert. "Google Chrome.app"
  // (The extracted leaf certificate is at /tmp/cert.0; intermediates and root
  // are at successive numbers.)
  //
  // Show some information about the exported certificates:
  // openssl x509 -inform DER -in /tmp/cert.0 -noout -text -fingerprint
  // (The "SHA1 Fingerprint" value printed by -fingerprint should match the
  // hash used in a codesign designated requirement after allowing for obvious
  // formatting differences.)

  const char* const kIdentifierMatches[] = {
#if defined(GOOGLE_CHROME_BUILD)
    "com.google.Chrome",
    "com.google.Chrome.canary",
#else
    "org.chromium.Chromium",
#endif
  };

  const char* const kLeafCertificateHashMatches[] = {
    // Only official released builds of Google Chrome have ever been signed
    // (with a certificate that anyone knows about or cares about).
#if defined(GOOGLE_CHROME_BUILD)
    // This is the new certificate that has not yet been used to sign Chrome,
    // but will be. Once used, the reauthorization code will become obsolete
    // until it's needed for some other purpose in the future.
    // Subject: UID=EQHXZ8M8AV, CN=Developer ID Application: Google Inc.,
    //     OU=EQHXZ8M8AV, O=Google Inc., C=US
    // Issuer: CN=Developer ID Certification Authority,
    //     OU=Apple Certification Authority, O=Apple Inc., C=US
    // Validity: 2012-04-26 14:10:10 UTC to 2017-04-27 14:10:10 UTC
    // "85cee8254216185620ddc8851c7a9fc4dfe120ef",

    // This certificate was used on 2011-12-20 and 2011-12-21, but the "since
    // 2010-07-19" one below was restored afterwards as an interim fix to the
    // Keychain authorization problem. See http://crbug.com/108238 and
    // http://crbug.com/62605.
    // Subject: C=US, ST=California, L=Mountain View, O=Google Inc,
    //     OU=Digital ID Class 3 - Java Object Signing, CN=Google Inc
    // Issuer: C=US, O=VeriSign, Inc., OU=VeriSign Trust Network,
    //     OU=Terms of use at https://www.verisign.com/rpa (c)10,
    //     CN=VeriSign Class 3 Code Signing 2010 CA
    // Validity: 2011-11-14 00:00:00 UTC to 2014-11-13 23:59:59 UTC
    "06c92bec3bbf32068cb9208563d004169448ee21",

    // This certificate has been used since 2010-07-19, except for the brief
    // period when the certificate above was used.
    // Subject: C=US, ST=California, L=Mountain View, O=Google Inc,
    //     OU=Digital ID Class 3 - Java Object Signing, CN=Google Inc
    // Issuer: C=US, O=VeriSign, Inc., OU=VeriSign Trust Network,
    //    OU=Terms of use at https://www.verisign.com/rpa (c)09,
    //    CN=VeriSign Class 3 Code Signing 2009-2 CA
    // Validity: 2010-02-22 00:00:00 UTC to 2012-02-22 23:59:59 UTC
    "9481882581d8178db8b1649c0eaa4f9eb11288f0",

    // This certificate was used for all public Chrome releases prior to
    // 2010-07-19.
    // Subject: C=US, ST=California, L=Mountain View, O=Google Inc,
    //     OU=Digital ID Class 3 - Netscape Object Signing, CN=Google Inc
    // Issuer: C=US, O=VeriSign, Inc., OU=VeriSign Trust Network,
    //     OU=Terms of use at https://www.verisign.com/rpa (c)04,
    //     CN=VeriSign Class 3 Code Signing 2004 CA
    // Validity: 2007-06-19 00:00:00 UTC to 2010-06-18 23:59:59 UTC
    "fe5008fe0da7a2033816752d6eafe95214f5a7e1",
#endif
  };

  std::vector<std::string> requirement_matches;
  requirement_matches.reserve(arraysize(kIdentifierMatches) *
                              ARRAYSIZE_UNSAFE(kLeafCertificateHashMatches));

  for (size_t identifier_index = 0;
       identifier_index < arraysize(kIdentifierMatches);
       ++identifier_index) {
    for (size_t leaf_certificate_hash_index = 0;
         leaf_certificate_hash_index <
             ARRAYSIZE_UNSAFE(kLeafCertificateHashMatches);
         ++leaf_certificate_hash_index) {
      requirement_matches.push_back(base::StringPrintf(
          "identifier \"%s\" and certificate leaf = H\"%s\"",
          kIdentifierMatches[identifier_index],
          kLeafCertificateHashMatches[leaf_certificate_hash_index]));
    }
  }

  if (application && base::mac::AmIBundled()) {
    std::string application_requirement =
        RequirementStringForApplication(application);
    requirement_matches.push_back(application_requirement);
  }

  return requirement_matches;
}

std::vector<CrSKeychainItemAndAccess> KCSearchToKCItemsAndReauthorizedAccesses(
    SecKeychainSearchRef search,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application) {
  std::vector<CrSKeychainItemAndAccess> items_and_accesses;

  base::mac::ScopedCFTypeRef<SecKeychainItemRef> item;
  while (item.reset(CrSKeychainSearchCopyNext(search)), item) {
    scoped_ptr<CrSKeychainItemAndAccess> item_and_access(
        KCItemToKCItemAndReauthorizedAccess(item,
                                            requirement_matches,
                                            this_application));

    if (item_and_access.get()) {
      items_and_accesses.push_back(*item_and_access);
    }
  }

  return items_and_accesses;
}

CrSKeychainItemAndAccess* KCItemToKCItemAndReauthorizedAccess(
    SecKeychainItemRef item,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application) {
  if (!CrSKeychainItemTestAccess(item)) {
    return NULL;
  }

  base::mac::ScopedCFTypeRef<SecAccessRef> access(
      CrSKeychainItemCopyAccess(item));
  base::mac::ScopedCFTypeRef<CFArrayRef> acl_list(
      CrSAccessCopyACLList(access));
  if (!acl_list) {
    return NULL;
  }

  bool acl_list_modified = ReauthorizeACLList(acl_list,
                                              requirement_matches,
                                              this_application);
  if (!acl_list_modified) {
    return NULL;
  }

  return new CrSKeychainItemAndAccess(item, access);
}

bool ReauthorizeACLList(
    CFArrayRef acl_list,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application) {
  bool acl_list_modified = false;

  CFIndex acl_count = CFArrayGetCount(acl_list);
  for (CFIndex acl_index = 0; acl_index < acl_count; ++acl_index) {
    SecACLRef acl = base::mac::CFCast<SecACLRef>(
        CFArrayGetValueAtIndex(acl_list, acl_index));
    if (!acl) {
      continue;
    }

    if (ReauthorizeACL(acl, requirement_matches, this_application)) {
      acl_list_modified = true;
    }
  }

  return acl_list_modified;
}

bool ReauthorizeACL(
    SecACLRef acl,
    const std::vector<std::string>& requirement_matches,
    SecTrustedApplicationRef this_application) {
  scoped_ptr<CrSACLSimpleContents> acl_simple_contents(
      CrSACLCopySimpleContents(acl));
  if (!acl_simple_contents.get() ||
      !acl_simple_contents->application_list) {
    return false;
  }

  CFMutableArrayRef application_list_mutable = NULL;
  bool added_this_application = false;

  CFIndex application_count =
      CFArrayGetCount(acl_simple_contents->application_list);
  for (CFIndex application_index = 0;
       application_index < application_count;
       ++application_index) {
    SecTrustedApplicationRef application =
        base::mac::CFCast<SecTrustedApplicationRef>(
            CFArrayGetValueAtIndex(acl_simple_contents->application_list,
                                   application_index));
    std::string requirement_string =
        RequirementStringForApplication(application);
    if (requirement_string.empty()) {
      continue;
    }

    if (std::find(requirement_matches.begin(),
                  requirement_matches.end(),
                  requirement_string) != requirement_matches.end()) {
      if (!application_list_mutable) {
        application_list_mutable =
            CFArrayCreateMutableCopy(NULL,
                                     application_count,
                                     acl_simple_contents->application_list);
        acl_simple_contents->application_list.reset(
            application_list_mutable);
      }

      if (!added_this_application) {
        CFArraySetValueAtIndex(application_list_mutable,
                               application_index,
                               this_application);
        added_this_application = true;
      } else {
        // Even though it's more bookkeeping to walk a list in the forward
        // direction when there are removals, it's done here anyway to
        // keep this_application at the position of the first match.
        CFArrayRemoveValueAtIndex(application_list_mutable,
                                  application_index);
        --application_index;
        --application_count;
      }
    }
  }

  if (!application_list_mutable) {
    return false;
  }

  if (!CrSACLSetSimpleContents(acl, *acl_simple_contents.get())) {
    return false;
  }

  return true;
}

void WriteKCItemsAndReauthorizedAccesses(
    const std::vector<CrSKeychainItemAndAccess>&
        items_and_reauthorized_accesses) {
  for (std::vector<CrSKeychainItemAndAccess>::const_iterator iterator =
           items_and_reauthorized_accesses.begin();
       iterator != items_and_reauthorized_accesses.end();
       ++iterator) {
    WriteKCItemAndReauthorizedAccess(*iterator);
  }
}

void WriteKCItemAndReauthorizedAccess(
    const CrSKeychainItemAndAccess& item_and_reauthorized_access) {
  SecKeychainItemRef old_item = item_and_reauthorized_access.item();
  base::mac::ScopedCFTypeRef<SecKeychainRef> keychain(
      CrSKeychainItemCopyKeychain(old_item));

  ScopedCrSKeychainItemAttributesAndData old_attributes_and_data(
      CrSKeychainItemCopyAttributesAndData(keychain, old_item));
  if (!old_attributes_and_data.get()) {
    return;
  }

  // CrSKeychainItemCreateFromContent (SecKeychainItemCreateFromContent)
  // returns errKCNoSuchAttr (errSecNoSuchAttr) when asked to add an item of
  // type kSecPrivateKeyItemClass. This would happen after the original
  // private key was deleted, resulting in data loss. I can't figure out how
  // SecKeychainItemCreateFromContent wants private keys added. Skip them,
  // only doing the reauthorization for Keychain item types known to work,
  // the item types expected to be used by most users and those that are
  // synced. See http://crbug.com/130738 and
  // http://lists.apple.com/archives/apple-cdsa/2006/Jan/msg00025.html .
  switch (old_attributes_and_data.item_class()) {
    case kSecInternetPasswordItemClass:
    case kSecGenericPasswordItemClass:
      break;
    default:
      return;
  }

  // SecKeychainItemCreateFromContent fails if any attribute is zero-length,
  // but old_attributes_and_data can contain zero-length attributes. Create
  // a new attribute list devoid of zero-length attributes.
  //
  // This is awkward: only the logic to build the
  // std::vector<SecKeychainAttribute> is in KCAttributesWithoutZeroLength
  // because the storage used for the new attribute list (the vector) needs to
  // persist through the lifetime of this function.
  // KCAttributesWithoutZeroLength doesn't return a
  // CrSKeychainItemAttributesAndData (which could be held here in a
  // ScopedCrSKeychainItemAttributesAndData) because it's more convenient to
  // build the attribute list using std::vector and point the data at the copy
  // in old_attributes_and_data, thus making nothing in new_attributes a
  // strongly-held reference.
  std::vector<SecKeychainAttribute> new_attributes =
      KCAttributesWithoutZeroLength(old_attributes_and_data.attribute_list());
  SecKeychainAttributeList new_attribute_list;
  new_attribute_list.count = new_attributes.size();
  new_attribute_list.attr =
      new_attribute_list.count ? &new_attributes[0] : NULL;
  CrSKeychainItemAttributesAndData new_attributes_and_data =
      *old_attributes_and_data.get();
  new_attributes_and_data.attribute_list = &new_attribute_list;

  // Delete the item last, to give everything else above a chance to bail
  // out early, and to ensure that the old item is still present while it
  // may still be used by the above code.
  if (!CrSKeychainItemDelete(old_item)) {
    return;
  }

  base::mac::ScopedCFTypeRef<SecKeychainItemRef> new_item(
      CrSKeychainItemCreateFromContent(new_attributes_and_data,
                                       keychain,
                                       item_and_reauthorized_access.access()));
}

std::vector<SecKeychainAttribute> KCAttributesWithoutZeroLength(
    SecKeychainAttributeList* old_attribute_list) {
  UInt32 old_attribute_count = old_attribute_list->count;
  std::vector<SecKeychainAttribute> new_attributes;
  new_attributes.reserve(old_attribute_count);
  for (UInt32 old_attribute_index = 0;
       old_attribute_index < old_attribute_count;
       ++old_attribute_index) {
    SecKeychainAttribute* attribute =
        &old_attribute_list->attr[old_attribute_index];
    if (attribute->length) {
      new_attributes.push_back(*attribute);
    }
  }

  return new_attributes;
}

}  // namespace

}  // namespace chrome
