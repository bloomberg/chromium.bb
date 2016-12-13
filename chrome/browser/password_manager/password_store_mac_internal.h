// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_INTERNAL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_INTERNAL_H_

#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "crypto/apple_keychain.h"

using crypto::AppleKeychain;

// Adapter that wraps a AppleKeychain and provides interaction in terms of
// PasswordForms instead of Keychain items.
class MacKeychainPasswordFormAdapter {
 public:
  // Creates an adapter for |keychain|. This class does not take ownership of
  // |keychain|, so the caller must make sure that the keychain outlives the
  // created object.
  explicit MacKeychainPasswordFormAdapter(const AppleKeychain* keychain);

  // Returns all keychain entries matching |signon_realm| and |scheme|.
  std::vector<std::unique_ptr<autofill::PasswordForm>> PasswordsFillingForm(
      const std::string& signon_realm,
      autofill::PasswordForm::Scheme scheme);

  // Returns true if there is the Keychain entry that matches |query_form| on
  // all of the fields that uniquely identify a Keychain item.
  bool HasPasswordExactlyMatchingForm(const autofill::PasswordForm& query_form);

  // Returns true if the keychain contains any items that are mergeable with
  // |query_form|. This is different from actually extracting the passwords
  // and checking the return count, since doing that would require reading the
  // passwords from the keychain, thus potentially triggering authorizaiton UI,
  // whereas this won't.
  bool HasPasswordsMergeableWithForm(
      const autofill::PasswordForm& query_form);

  // Returns all keychain items of types corresponding to password forms.
  std::vector<SecKeychainItemRef> GetAllPasswordFormKeychainItems();

  // Returns all keychain entries corresponding to password forms.
  // TODO(vabr): This is only used in tests, should be moved there.
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  GetAllPasswordFormPasswords();

  // Creates a new keychain entry from |form|, or updates the password of an
  // existing keychain entry if there is a collision. Returns true if a keychain
  // entry was successfully added/updated.
  bool AddPassword(const autofill::PasswordForm& form);

  // Removes the keychain password matching |form| if any. Returns true if a
  // keychain item was found and successfully removed.
  bool RemovePassword(const autofill::PasswordForm& form);

  // Controls whether or not Chrome will restrict Keychain searches to items
  // that it created. Defaults to false.
  void SetFindsOnlyOwnedItems(bool finds_only_owned);

 private:
  // Returns PasswordForm instances transformed from |items|. Also calls
  // AppleKeychain::Free on all of the keychain items and clears |items|.
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  ConvertKeychainItemsToForms(std::vector<SecKeychainItemRef>* items);

  // Searches |keychain| for the specific keychain entry that corresponds to the
  // given form, and returns it (or NULL if no match is found). The caller is
  // responsible for calling AppleKeychain::Free on on the returned item.
  SecKeychainItemRef KeychainItemForForm(
      const autofill::PasswordForm& form);

  // Returns the Keychain items matching the given signon_realm, scheme, and
  // optionally path and username (either of both can be NULL).
  // The caller is responsible for calling AppleKeychain::Free on the
  // returned items.
  std::vector<SecKeychainItemRef> MatchingKeychainItems(
      const std::string& signon_realm,
      autofill::PasswordForm::Scheme scheme,
      const char* path,
      const char* username);

  // Returns the Keychain SecAuthenticationType type corresponding to |scheme|.
  SecAuthenticationType AuthTypeForScheme(
      autofill::PasswordForm::Scheme scheme);

  // Changes the password for keychain_item to |password|; returns true if the
  // password was successfully changed.
  bool SetKeychainItemPassword(SecKeychainItemRef keychain_item,
                               const std::string& password);

  // Sets the creator code of keychain_item to creator_code; returns true if the
  // creator code was successfully set.
  bool SetKeychainItemCreatorCode(SecKeychainItemRef keychain_item,
                                  OSType creator_code);

  // Returns the creator code to be used for a Keychain search, depending on
  // whether this object was instructed to search only for items it created.
  // If searches should be restricted in this way, the application-specific
  // creator code will be returned. Otherwise, 0 will be returned, indicating
  // a search of all items, regardless of creator.
  OSType CreatorCodeForSearch();

  const AppleKeychain* keychain_;

  // If true, Keychain searches are restricted to items created by Chrome.
  bool finds_only_owned_;

  DISALLOW_COPY_AND_ASSIGN(MacKeychainPasswordFormAdapter);
};

namespace internal_keychain_helpers {

// Pair of pointers to a SecKeychainItemRef and a corresponding PasswordForm.
typedef std::pair<SecKeychainItemRef, std::unique_ptr<autofill::PasswordForm>>
    ItemFormPair;

// Sets the fields of |form| based on the keychain data from |keychain_item|.
// Fields that can't be determined from |keychain_item| will be unchanged. If
// |extract_password_data| is true, the password data will be copied from
// |keychain_item| in addition to its attributes, and the |blacklisted_by_user|
// field will be set to true for empty passwords ("" or " ").
// If |extract_password_data| is false, only the password attributes will be
// copied, and the |blacklisted_by_user| field will always be false.
//
// IMPORTANT: If |extract_password_data| is true, this function can cause the OS
// to trigger UI (to allow access to the keychain item if we aren't trusted for
// the item), and block until the UI is dismissed.
//
// If excessive prompting for access to other applications' keychain items
// becomes an issue, the password storage API will need to intially call this
// function with |extract_password_data| set to false, and retrieve the password
// later (accessing other fields doesn't require authorization).
bool FillPasswordFormFromKeychainItem(const AppleKeychain& keychain,
                                      SecKeychainItemRef keychain_item,
                                      autofill::PasswordForm* form,
                                      bool extract_password_data);

// Returns true if |keychain_item| has the application-specific creator code in
// its attributes.
bool HasCreatorCode(const AppleKeychain& keychain,
                    SecKeychainItemRef keychain_item);

// Use FormMatchStrictness to configure which forms are considered a match by
// FormsMatchForMerge:
enum FormMatchStrictness {
  STRICT_FORM_MATCH,  // Match only forms with the same scheme, signon realm and
                      // username value.
  FUZZY_FORM_MATCH,   // Also match cases where the first form's
                      // original_signon_realm is nonempty and matches the
                      // second form's signon_realm.
};

// Returns true if the two given forms are suitable for merging (see
// MergePasswordForms).
bool FormsMatchForMerge(const autofill::PasswordForm& form_a,
                        const autofill::PasswordForm& form_b,
                        FormMatchStrictness strictness);

// Populates merged_forms by combining the password data from keychain_forms and
// the metadata from database_forms, removing used entries from the two source
// lists.
//
// On return, database_forms and keychain_forms will have only unused
// entries; for database_forms that means entries for which no corresponding
// password can be found (and which aren't blacklist entries), and for
// keychain_forms its entries that weren't merged into at least one database
// form.
void MergePasswordForms(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* keychain_forms,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* database_forms,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* merged_forms);

// For every form in |database_forms|, if such a form has a corresponding entry
// in |keychain|, this adds the password from the entry and moves that form from
// |database_forms| into |passwords|.
void GetPasswordsForForms(
    const AppleKeychain& keychain,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* database_forms,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* passwords);

// Loads all items in the system keychain into |keychain_items|, creates for
// each keychain item a corresponding PasswordForm that doesn't contain any
// password data, and returns the two collections as a vector of ItemFormPairs.
// Used by GetPasswordsForForms for optimized matching of keychain items with
// PasswordForms in the database.
// Note: Since no password data is loaded here, the resulting PasswordForms
// will include blacklist entries, which will have to be filtered out later.
// Caller owns the SecKeychainItemRefs and PasswordForms that are returned.
// This operation does not require OS authorization.
std::vector<ItemFormPair> ExtractAllKeychainItemAttributesIntoPasswordForms(
    std::vector<SecKeychainItemRef>* keychain_items,
    const AppleKeychain& keychain);

// Takes a PasswordForm's signon_realm and parses it into its component parts,
// which are returned though the appropriate out parameters.
// Returns true if it can be successfully parsed, in which case all out params
// that are non-NULL will be set. If there is no port, port will be 0.
// If the return value is false, the state of the out params is undefined.
bool ExtractSignonRealmComponents(const std::string& signon_realm,
                                  std::string* server,
                                  UInt32* port,
                                  bool* is_secure,
                                  std::string* security_domain);

// Returns true if the signon_realm of |query_form| can be successfully parsed
// by ExtractSignonRealmComponents, and if |query_form| matches |other_form|.
bool FormIsValidAndMatchesOtherForm(const autofill::PasswordForm& query_form,
                                    const autofill::PasswordForm& other_form);

// Returns PasswordForm instances populated with password data for each keychain
// entry in |item_form_pairs| that could be merged with |query_form|.
std::vector<std::unique_ptr<autofill::PasswordForm>>
ExtractPasswordsMergeableWithForm(
    const AppleKeychain& keychain,
    const std::vector<ItemFormPair>& item_form_pairs,
    const autofill::PasswordForm& query_form);

}  // namespace internal_keychain_helpers

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_INTERNAL_H_
