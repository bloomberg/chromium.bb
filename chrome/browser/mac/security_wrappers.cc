// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/security_wrappers.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"

extern "C" {
OSStatus SecTrustedApplicationCopyRequirement(
    SecTrustedApplicationRef application,
    SecRequirementRef* requirement);
}  // extern "C"

namespace chrome {

ScopedSecKeychainSetUserInteractionAllowed::
    ScopedSecKeychainSetUserInteractionAllowed(Boolean allowed) {
  OSStatus status = SecKeychainGetUserInteractionAllowed(&old_allowed_);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    old_allowed_ = TRUE;
  }

  status = SecKeychainSetUserInteractionAllowed(allowed);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
  }
}

ScopedSecKeychainSetUserInteractionAllowed::
    ~ScopedSecKeychainSetUserInteractionAllowed() {
  OSStatus status = SecKeychainSetUserInteractionAllowed(old_allowed_);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
  }
}

CrSKeychainItemAndAccess::CrSKeychainItemAndAccess(SecKeychainItemRef item,
                                                   SecAccessRef access)
    : item_(item),
      access_(access) {
  // These CFRetain calls aren't leaks. They're balanced by an implicit
  // CFRelease at destruction because the fields are of type ScopedCFTypeRef.
  // These fields are retained on construction (unlike the typical
  // ScopedCFTypeRef pattern) because this class is intended for use as an STL
  // type adapter to keep two related objects together, and thus must
  // implement proper reference counting in the methods required for STL
  // container use. This class and is not intended to act as a scoper for the
  // underlying objects in user code. For that, just use ScopedCFTypeRef.
  CFRetain(item_);
  CFRetain(access_);
}

CrSKeychainItemAndAccess::CrSKeychainItemAndAccess(
    const CrSKeychainItemAndAccess& that)
    : item_(that.item_.get()),
      access_(that.access_.get()) {
  // See the comment above in the two-argument constructor.
  CFRetain(item_);
  CFRetain(access_);
}

CrSKeychainItemAndAccess::~CrSKeychainItemAndAccess() {
}

void CrSKeychainItemAndAccess::operator=(const CrSKeychainItemAndAccess& that) {
  // See the comment above in the two-argument constructor.
  CFRetain(that.item_);
  item_.reset(that.item_);

  CFRetain(that.access_);
  access_.reset(that.access_);
}

CrSACLSimpleContents::CrSACLSimpleContents() {
}

CrSACLSimpleContents::~CrSACLSimpleContents() {
}

ScopedSecKeychainAttributeInfo::ScopedSecKeychainAttributeInfo(
    SecKeychainAttributeInfo* attribute_info)
    : attribute_info_(attribute_info) {
}

ScopedSecKeychainAttributeInfo::~ScopedSecKeychainAttributeInfo() {
  OSStatus status = SecKeychainFreeAttributeInfo(attribute_info_);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
  }
}

ScopedCrSKeychainItemAttributesAndData::ScopedCrSKeychainItemAttributesAndData(
    CrSKeychainItemAttributesAndData* attributes_and_data)
    : attributes_and_data_(attributes_and_data) {
}

ScopedCrSKeychainItemAttributesAndData::
    ~ScopedCrSKeychainItemAttributesAndData() {
  if (attributes_and_data_.get()) {
    CrSKeychainItemFreeAttributesAndData(
        attributes_and_data_->attribute_list, attributes_and_data_->data);
  }
}

SecKeychainSearchRef CrSKeychainSearchCreateFromAttributes(
    CFTypeRef keychain_or_array,
    SecItemClass item_class,
    const SecKeychainAttributeList* attribute_list) {
  SecKeychainSearchRef search;
  OSStatus status = SecKeychainSearchCreateFromAttributes(keychain_or_array,
                                                          item_class,
                                                          attribute_list,
                                                          &search);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return search;
}

SecKeychainItemRef CrSKeychainSearchCopyNext(SecKeychainSearchRef search) {
  if (!search) {
    return NULL;
  }

  SecKeychainItemRef item;
  OSStatus status = SecKeychainSearchCopyNext(search, &item);
  if (status != errSecSuccess) {
    if (status != errSecItemNotFound) {
      OSSTATUS_LOG(ERROR, status);
    }
    return NULL;
  }

  return item;
}

void CrSKeychainItemFreeAttributesAndData(
    SecKeychainAttributeList* attribute_list,
    void* data) {
  OSStatus status = SecKeychainItemFreeAttributesAndData(attribute_list, data);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
  }
}

bool CrSKeychainItemTestAccess(SecKeychainItemRef item) {
  UInt32 length;
  void* data;
  OSStatus status = SecKeychainItemCopyAttributesAndData(item,
                                                         NULL,
                                                         NULL,
                                                         NULL,
                                                         &length,
                                                         &data);
  if (status != errSecSuccess) {
    if (status != errSecAuthFailed) {
      OSSTATUS_LOG(ERROR, status);
    }
    return false;
  }

  CrSKeychainItemFreeAttributesAndData(NULL, data);

  return true;
}

SecAccessRef CrSKeychainItemCopyAccess(SecKeychainItemRef item) {
  SecAccessRef access;
  OSStatus status = SecKeychainItemCopyAccess(item, &access);
  if (status != errSecSuccess) {
    if (status != errSecNoAccessForItem && status != errSecAuthFailed) {
      OSSTATUS_LOG(ERROR, status);
    }
    return NULL;
  }

  return access;
}

CFArrayRef CrSAccessCopyACLList(SecAccessRef access) {
  if (!access) {
    return NULL;
  }

  CFArrayRef acl_list;
  OSStatus status = SecAccessCopyACLList(access, &acl_list);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return acl_list;
}

CrSACLSimpleContents* CrSACLCopySimpleContents(SecACLRef acl) {
  if (!acl) {
    return NULL;
  }

  scoped_ptr<CrSACLSimpleContents> acl_simple_contents(
      new CrSACLSimpleContents());
  CFArrayRef application_list;
  CFStringRef description;
  OSStatus status =
      SecACLCopySimpleContents(acl,
                               &application_list,
                               &description,
                               &acl_simple_contents->prompt_selector);
  if (status != errSecSuccess) {
    if (status != errSecACLNotSimple) {
      OSSTATUS_LOG(ERROR, status);
    }
    return NULL;
  }

  acl_simple_contents->application_list.reset(application_list);
  acl_simple_contents->description.reset(description);

  return acl_simple_contents.release();
}

SecRequirementRef CrSTrustedApplicationCopyRequirement(
    SecTrustedApplicationRef application) {
  if (!application) {
    return NULL;
  }

  SecRequirementRef requirement;
  OSStatus status = SecTrustedApplicationCopyRequirement(application,
                                                         &requirement);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return requirement;
}

CFStringRef CrSRequirementCopyString(SecRequirementRef requirement,
                                     SecCSFlags flags) {
  if (!requirement) {
    return NULL;
  }

  CFStringRef requirement_string;
  OSStatus status = SecRequirementCopyString(requirement,
                                             flags,
                                             &requirement_string);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return requirement_string;
}

SecTrustedApplicationRef CrSTrustedApplicationCreateFromPath(const char* path) {
  SecTrustedApplicationRef application;
  OSStatus status = SecTrustedApplicationCreateFromPath(path, &application);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return application;
}

bool CrSACLSetSimpleContents(SecACLRef acl,
                             const CrSACLSimpleContents& acl_simple_contents) {
  OSStatus status =
      SecACLSetSimpleContents(acl,
                              acl_simple_contents.application_list,
                              acl_simple_contents.description,
                              &acl_simple_contents.prompt_selector);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return false;
  }

  return true;
}

SecKeychainRef CrSKeychainItemCopyKeychain(SecKeychainItemRef item) {
  SecKeychainRef keychain;
  OSStatus status = SecKeychainItemCopyKeychain(item, &keychain);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return keychain;
}

SecKeychainAttributeInfo* CrSKeychainAttributeInfoForItemID(
    SecKeychainRef keychain,
    UInt32 item_id) {
  SecKeychainAttributeInfo* attribute_info;
  OSStatus status = SecKeychainAttributeInfoForItemID(keychain,
                                                      item_id,
                                                      &attribute_info);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return attribute_info;
}

CrSKeychainItemAttributesAndData* CrSKeychainItemCopyAttributesAndData(
    SecKeychainRef keychain,
    SecKeychainItemRef item) {
  ScopedCrSKeychainItemAttributesAndData attributes_and_data(
      new CrSKeychainItemAttributesAndData());
  OSStatus status =
      SecKeychainItemCopyAttributesAndData(item,
                                           NULL,
                                           attributes_and_data.item_class_ptr(),
                                           NULL,
                                           NULL,
                                           NULL);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  // This looks really weird, but it's right. See 10.7.3
  // libsecurity_keychain-55044 lib/SecItem.cpp
  // _CreateAttributesDictionaryFromKeyItem and 10.7.3 SecurityTool-55002
  // keychain_utilities.c print_keychain_item_attributes.
  UInt32 item_id;
  switch (attributes_and_data.item_class()) {
    case kSecInternetPasswordItemClass:
      item_id = CSSM_DL_DB_RECORD_INTERNET_PASSWORD;
      break;
    case kSecGenericPasswordItemClass:
      item_id = CSSM_DL_DB_RECORD_GENERIC_PASSWORD;
      break;
    // kSecInternetPasswordItemClass is marked as deprecated in the 10.9 sdk,
    // but the files in libsecurity_keychain from 10.7 referenced above still
    // use it. Also see rdar://14281375 /
    // http://openradar.appspot.com/radar?id=3143412 .
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    case kSecAppleSharePasswordItemClass:
#pragma clang diagnostic pop
      item_id = CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD;
      break;
    default:
      item_id = attributes_and_data.item_class();
      break;
  }

  ScopedSecKeychainAttributeInfo attribute_info(
      CrSKeychainAttributeInfoForItemID(keychain, item_id));
  if (!attribute_info) {
    return NULL;
  }

  status = SecKeychainItemCopyAttributesAndData(
      item,
      attribute_info,
      attributes_and_data.item_class_ptr(),
      attributes_and_data.attribute_list_ptr(),
      attributes_and_data.length_ptr(),
      attributes_and_data.data_ptr());
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return attributes_and_data.release();
}

bool CrSKeychainItemDelete(SecKeychainItemRef item) {
  OSStatus status = SecKeychainItemDelete(item);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return false;
  }

  return true;
}

SecKeychainItemRef CrSKeychainItemCreateFromContent(
    const CrSKeychainItemAttributesAndData& attributes_and_data,
    SecKeychainRef keychain,
    SecAccessRef access) {
  SecKeychainItemRef item;
  OSStatus status =
      SecKeychainItemCreateFromContent(attributes_and_data.item_class,
                                       attributes_and_data.attribute_list,
                                       attributes_and_data.length,
                                       attributes_and_data.data,
                                       keychain,
                                       access,
                                       &item);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    return NULL;
  }

  return item;
}

}  // namespace chrome
