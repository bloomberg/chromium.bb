// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_SECURITY_WRAPPERS_H_
#define CHROME_BROWSER_MAC_SECURITY_WRAPPERS_H_

#include <Security/Security.h>
#include <Security/SecRequirement.h>

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"

namespace chrome {

// Wraps SecKeychainSetUserInteractionAllowed, restoring the previous setting
// on destruction.
class ScopedSecKeychainSetUserInteractionAllowed {
 public:
  explicit ScopedSecKeychainSetUserInteractionAllowed(Boolean allowed);
  ~ScopedSecKeychainSetUserInteractionAllowed();

 private:
  Boolean old_allowed_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSecKeychainSetUserInteractionAllowed);
};

// Holds a paired SecKeychainItemRef and SecAccessRef, maintaining the
// association between the two, and managing their ownership by retaining
// the SecKeychainItemRef and SecAccessRef elements placed into a
// CrSKeychainItemAndAccess object. Suitable for use
// in standard C++ containers.
class CrSKeychainItemAndAccess {
 public:
  CrSKeychainItemAndAccess(SecKeychainItemRef item, SecAccessRef access);
  CrSKeychainItemAndAccess(const CrSKeychainItemAndAccess& that);

  ~CrSKeychainItemAndAccess();

  void operator=(const CrSKeychainItemAndAccess& that);

  SecKeychainItemRef item() const { return item_; }
  SecAccessRef access() const { return access_; }

 private:
  base::ScopedCFTypeRef<SecKeychainItemRef> item_;
  base::ScopedCFTypeRef<SecAccessRef> access_;
};

// Holds the return value from CrSACLCopySimpleContents and an argument to
// CrSACLSetSimpleContents, managing ownership. Used in those wrappers to keep
// logically grouped data together.
struct CrSACLSimpleContents {
  CrSACLSimpleContents();
  ~CrSACLSimpleContents();

  base::ScopedCFTypeRef<CFArrayRef> application_list;
  base::ScopedCFTypeRef<CFStringRef> description;
  CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR prompt_selector;
};

// Holds a SecKeychainAttributeInfo*, calling SecKeychainFreeAttributeInfo on
// destruction.
class ScopedSecKeychainAttributeInfo {
 public:
  explicit ScopedSecKeychainAttributeInfo(
      SecKeychainAttributeInfo* attribute_info);
  ~ScopedSecKeychainAttributeInfo();

  operator SecKeychainAttributeInfo*() const {
    return attribute_info_;
  }

 private:
  SecKeychainAttributeInfo* attribute_info_;
};

// Holds the return value from CrSKeychainItemCopyAttributesAndData and an
// argument to CrSKeychainItemCreateFromContent. Used in those wrappers to
// keep logically grouped data together.
struct CrSKeychainItemAttributesAndData {
  SecItemClass item_class;
  SecKeychainAttributeList* attribute_list;
  UInt32 length;
  void* data;
};

// Holds a CrSKeychainItemAttributesAndData*, calling
// CrSKeychainItemFreeAttributesAndData and freeing the owned
// CrSKeychainItemAttributesAndData* on destruction.
class ScopedCrSKeychainItemAttributesAndData {
 public:
  ScopedCrSKeychainItemAttributesAndData(
      CrSKeychainItemAttributesAndData* attributes_and_data);
  ~ScopedCrSKeychainItemAttributesAndData();

  CrSKeychainItemAttributesAndData* get() const {
    return attributes_and_data_.get();
  }

  CrSKeychainItemAttributesAndData* release() {
    return attributes_and_data_.release();
  }

  SecItemClass item_class() const {
    return attributes_and_data_->item_class;
  }

  SecItemClass* item_class_ptr() const {
    return &attributes_and_data_->item_class;
  }

  SecKeychainAttributeList* attribute_list() const {
    return attributes_and_data_->attribute_list;
  }

  SecKeychainAttributeList** attribute_list_ptr() const {
    return &attributes_and_data_->attribute_list;
  }

  UInt32 length() const {
    return attributes_and_data_->length;
  }

  UInt32* length_ptr() const {
    return &attributes_and_data_->length;
  }

  void* data() const {
    return attributes_and_data_->data;
  }

  void** data_ptr() const {
    return &attributes_and_data_->data;
  }

 private:
  scoped_ptr<CrSKeychainItemAttributesAndData> attributes_and_data_;
};

// Wraps SecKeychainSearchCreateFromAttributes, returning NULL on error and a
// SecKeychainSearchRef owned by the caller on success.
SecKeychainSearchRef CrSKeychainSearchCreateFromAttributes(
    CFTypeRef keychain_or_array,
    SecItemClass item_class,
    const SecKeychainAttributeList* attribute_list);

// Wraps SecKeychainSearchCopyNext, tolerating a NULL argument (resulting in
// a NULL return value but nothing logged), returning NULL on error and a
// SecKeychainItemRef owned by the caller on success.
SecKeychainItemRef CrSKeychainSearchCopyNext(SecKeychainSearchRef search);

// Wraps SecKeychainItemFreeAttributesAndData.
void CrSKeychainItemFreeAttributesAndData(
    SecKeychainAttributeList* attribute_list,
    void* data);

// Tests access to |item| by calling SecKeychainItemCopyAttributesAndData,
// taking care to properly free any returned data. Returns true if access to
// |item| is authorized. errSecAuthFailed is considered an "expected" error
// for which nothing will be logged, although false will be returned.
bool CrSKeychainItemTestAccess(SecKeychainItemRef item);

// Wraps SecKeychainItemCopyAccess, returning NULL on error and a SecAccessRef
// owned by the caller on success. errSecNoAccessForItem and errSecAuthFailed
// are considered "expected" errors for which nothing will be logged, although
// NULL will be returned.
SecAccessRef CrSKeychainItemCopyAccess(SecKeychainItemRef item);

// Wraps SecAccessCopyACLList, returning NULL on error and a CFArrayRef owned
// by the caller on success.
CFArrayRef CrSAccessCopyACLList(SecAccessRef access);

// Wraps SecACLCopySimpleContents, returning NULL on error and a
// CrSACLSimpleContents* owned by the caller on success. errSecACLNotSimple is
// considered an "expected" error for which nothing will be logged, although
// NULL will be returned.
CrSACLSimpleContents* CrSACLCopySimpleContents(SecACLRef acl);

// Wraps SecTrustedApplicationCopyRequirement, tolerating a NULL argument
// (resulting in a NULL return value but nothing logged) and returning NULL on
// error or a SecRequirementRef owned by the caller on success.
SecRequirementRef CrSTrustedApplicationCopyRequirement(
    SecTrustedApplicationRef application);

// Wraps SecRequirementCopyString, tolerating a NULL argument (resulting in
// a NULL return value but nothing logged) and returning NULL on error or a
// CFStringRef owned by the caller on success.
CFStringRef CrSRequirementCopyString(SecRequirementRef requirement,
                                     SecCSFlags flags);

// Wraps SecTrustedApplicationCreateFromPath, returning NULL on error or a
// SecTrustedApplicationRef owned by the caller on success.
SecTrustedApplicationRef CrSTrustedApplicationCreateFromPath(const char* path);

// Wraps SecACLSetSimpleContents, adapting it to the CrSACLSimpleContents
// argument, returning false on error or true on success.
bool CrSACLSetSimpleContents(SecACLRef acl,
                             const CrSACLSimpleContents& acl_simple_contents);

// Wraps SecKeychainItemCopyKeychain, returning NULL on error or a
// SecKeychainRef owned by the caller on success.
SecKeychainRef CrSKeychainItemCopyKeychain(SecKeychainItemRef item);

// Wraps SecKeychainAttributeInfoForItemID, returning NULL on error or a
// SecKeychainAttributeInfo* owned by the caller on success.
SecKeychainAttributeInfo* CrSKeychainAttributeInfoForItemID(
    SecKeychainRef keychain,
    UInt32 item_id);

// Wraps SecKeychainItemCopyAttributesAndData, returning NULL on error or a
// CrSKeychainItemAttributesAndData* owned by the caller on success.
CrSKeychainItemAttributesAndData* CrSKeychainItemCopyAttributesAndData(
    SecKeychainRef keychain,
    SecKeychainItemRef item);

// Wraps SecKeychainItemDelete, returning false on error or true on success.
bool CrSKeychainItemDelete(SecKeychainItemRef item);

// Wraps SecKeychainItemCreateFromContent, adapting it to the
// CrSKeychainItemAttributesAndData argument, returning NULL on error or a
// SecKeychainItemRef owned by the caller on success.
SecKeychainItemRef CrSKeychainItemCreateFromContent(
    const CrSKeychainItemAttributesAndData& attributes_and_data,
    SecKeychainRef keychain,
    SecAccessRef access);

}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_SECURITY_WRAPPERS_H_
