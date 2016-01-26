// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"

#include <CoreServices/CoreServices.h>
#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/mac/security_wrappers.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/apple_keychain.h"
#include "url/origin.h"

using autofill::PasswordForm;
using crypto::AppleKeychain;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;

namespace {

// Utility class to handle the details of constructing and running a keychain
// search from a set of attributes.
class KeychainSearch {
 public:
  explicit KeychainSearch(const AppleKeychain& keychain);
  ~KeychainSearch();

  // Sets up a keycahin search based on an non "null" (NULL for char*,
  // The appropriate "Any" entry for other types) arguments.
  //
  // IMPORTANT: Any paramaters passed in *must* remain valid for as long as the
  // KeychainSearch object, since the search uses them by reference.
  void Init(const char* server,
            const UInt32* port,
            const SecProtocolType* protocol,
            const SecAuthenticationType* auth_type,
            const char* security_domain,
            const char* path,
            const char* username,
            const OSType* creator);

  // Fills |items| with all Keychain items that match the Init'd search.
  // If the search fails for any reason, |items| will be unchanged.
  void FindMatchingItems(std::vector<SecKeychainItemRef>* matches);

 private:
  const AppleKeychain* keychain_;
  SecKeychainAttributeList search_attributes_;
  SecKeychainSearchRef search_ref_;
};

KeychainSearch::KeychainSearch(const AppleKeychain& keychain)
    : keychain_(&keychain), search_ref_(NULL) {
  search_attributes_.count = 0;
  search_attributes_.attr = NULL;
}

KeychainSearch::~KeychainSearch() {
  if (search_attributes_.attr) {
    free(search_attributes_.attr);
  }
}

void KeychainSearch::Init(const char* server,
                          const UInt32* port,
                          const SecProtocolType* protocol,
                          const SecAuthenticationType* auth_type,
                          const char* security_domain,
                          const char* path,
                          const char* username,
                          const OSType* creator) {
  // Allocate enough to hold everything we might use.
  const unsigned int kMaxEntryCount = 8;
  search_attributes_.attr =
      static_cast<SecKeychainAttribute*>(calloc(kMaxEntryCount,
                                                sizeof(SecKeychainAttribute)));
  unsigned int entries = 0;
  // We only use search_attributes_ with SearchCreateFromAttributes, which takes
  // a "const SecKeychainAttributeList *", so we trust that they won't try
  // to modify the list, and that casting away const-ness is thus safe.
  if (server != NULL) {
    DCHECK_LT(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecServerItemAttr;
    search_attributes_.attr[entries].length = strlen(server);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(server));
    ++entries;
  }
  if (port != NULL && *port != kAnyPort) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecPortItemAttr;
    search_attributes_.attr[entries].length = sizeof(*port);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(port));
    ++entries;
  }
  if (protocol != NULL && *protocol != kSecProtocolTypeAny) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecProtocolItemAttr;
    search_attributes_.attr[entries].length = sizeof(*protocol);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(protocol));
    ++entries;
  }
  if (auth_type != NULL && *auth_type != kSecAuthenticationTypeAny) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecAuthenticationTypeItemAttr;
    search_attributes_.attr[entries].length = sizeof(*auth_type);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(auth_type));
    ++entries;
  }
  if (security_domain != NULL && strlen(security_domain) > 0) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecSecurityDomainItemAttr;
    search_attributes_.attr[entries].length = strlen(security_domain);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(security_domain));
    ++entries;
  }
  if (path != NULL && strlen(path) > 0 && strcmp(path, "/") != 0) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecPathItemAttr;
    search_attributes_.attr[entries].length = strlen(path);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(path));
    ++entries;
  }
  if (username != NULL) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecAccountItemAttr;
    search_attributes_.attr[entries].length = strlen(username);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(username));
    ++entries;
  }
  if (creator != NULL) {
    DCHECK_LE(entries, kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecCreatorItemAttr;
    search_attributes_.attr[entries].length = sizeof(*creator);
    search_attributes_.attr[entries].data =
        const_cast<void*>(static_cast<const void*>(creator));
    ++entries;
  }
  search_attributes_.count = entries;
}

void KeychainSearch::FindMatchingItems(std::vector<SecKeychainItemRef>* items) {
  OSStatus result = keychain_->SearchCreateFromAttributes(
      NULL, kSecInternetPasswordItemClass, &search_attributes_, &search_ref_);

  if (result != noErr) {
    OSSTATUS_LOG(ERROR, result) << "Keychain lookup failed";
    return;
  }

  SecKeychainItemRef keychain_item;
  while (keychain_->SearchCopyNext(search_ref_, &keychain_item) == noErr) {
    // Consumer is responsible for freeing the items.
    items->push_back(keychain_item);
  }

  keychain_->Free(search_ref_);
  search_ref_ = NULL;
}

PasswordStoreChangeList FormsToRemoveChangeList(
    const std::vector<PasswordForm*>& forms) {
  PasswordStoreChangeList changes;
  for (std::vector<PasswordForm*>::const_iterator i = forms.begin();
       i != forms.end(); ++i) {
    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, **i));
  }
  return changes;
}

// Moves the content of |second| to the end of |first|.
void AppendSecondToFirst(ScopedVector<autofill::PasswordForm>* first,
                         ScopedVector<autofill::PasswordForm>* second) {
  first->insert(first->end(), second->begin(), second->end());
  second->weak_clear();
}

// Returns the best match for |base_form| from |keychain_forms|, or nullptr if
// there is no suitable match.
const PasswordForm* BestKeychainFormForForm(
    const PasswordForm& base_form,
    const std::vector<PasswordForm*>& keychain_forms) {
  const PasswordForm* partial_match = nullptr;
  for (const auto* keychain_form : keychain_forms) {
    // TODO(stuartmorgan): We should really be scoring path matches and picking
    // the best, rather than just checking exact-or-not (although in practice
    // keychain items with paths probably came from us).
    if (internal_keychain_helpers::FormsMatchForMerge(
            base_form, *keychain_form,
            internal_keychain_helpers::FUZZY_FORM_MATCH)) {
      if (base_form.origin == keychain_form->origin) {
        return keychain_form;
      } else if (!partial_match) {
        partial_match = keychain_form;
      }
    }
  }
  return partial_match;
}

// Iterates over all elements in |forms|, passes the pointed to objects to
// |mover|, and clears |forms| efficiently. FormMover needs to be a callable
// entity, accepting scoped_ptr<autofill::PasswordForm> as its sole argument.
template <typename FormMover>
inline void MoveAllFormsOut(ScopedVector<autofill::PasswordForm>* forms,
                            FormMover mover) {
  for (autofill::PasswordForm* form_ptr : *forms) {
    mover(scoped_ptr<autofill::PasswordForm>(form_ptr));
  }
  // We moved the ownership of every form out of |forms|. For performance
  // reasons, we can just weak_clear it, instead of nullptr-ing the respective
  // elements and letting the vector's destructor to go through the list once
  // more. This was tested on a benchmark, and seemed to make a difference on
  // Mac.
  forms->weak_clear();
}

// True if the form has no password to be stored in Keychain.
bool IsLoginDatabaseOnlyForm(const autofill::PasswordForm& form) {
  return form.blacklisted_by_user || !form.federation_url.is_empty() ||
         form.scheme == autofill::PasswordForm::SCHEME_USERNAME_ONLY;
}

}  // namespace

#pragma mark -

// TODO(stuartmorgan): Convert most of this to private helpers in
// MacKeychainPasswordFormAdapter once it has sufficient higher-level public
// methods to provide test coverage.
namespace internal_keychain_helpers {

// Returns a URL built from the given components. To create a URL without a
// port, pass kAnyPort for the |port| parameter.
GURL URLFromComponents(bool is_secure, const std::string& host, int port,
                       const std::string& path) {
  GURL::Replacements url_components;
  std::string scheme(is_secure ? "https" : "http");
  url_components.SetSchemeStr(scheme);
  url_components.SetHostStr(host);
  std::string port_string;  // Must remain in scope until after we do replacing.
  if (port != kAnyPort) {
    std::ostringstream port_stringstream;
    port_stringstream << port;
    port_string = port_stringstream.str();
    url_components.SetPortStr(port_string);
  }
  url_components.SetPathStr(path);

  GURL url("http://dummy.com");  // ReplaceComponents needs a valid URL.
  return url.ReplaceComponents(url_components);
}

// Converts a Keychain time string to a Time object, returning true if
// time_string_bytes was parsable. If the return value is false, the value of
// |time| is unchanged.
bool TimeFromKeychainTimeString(const char* time_string_bytes,
                                unsigned int byte_length,
                                base::Time* time) {
  DCHECK(time);

  char* time_string = static_cast<char*>(malloc(byte_length + 1));
  memcpy(time_string, time_string_bytes, byte_length);
  time_string[byte_length] = '\0';
  base::Time::Exploded exploded_time;
  bzero(&exploded_time, sizeof(exploded_time));
  // The time string is of the form "yyyyMMddHHmmss'Z", in UTC time.
  int assignments = sscanf(time_string, "%4d%2d%2d%2d%2d%2dZ",
                           &exploded_time.year, &exploded_time.month,
                           &exploded_time.day_of_month, &exploded_time.hour,
                           &exploded_time.minute, &exploded_time.second);
  free(time_string);

  if (assignments == 6) {
    *time = base::Time::FromUTCExploded(exploded_time);
    return true;
  }
  return false;
}

// Returns the PasswordForm Scheme corresponding to |auth_type|.
PasswordForm::Scheme SchemeForAuthType(SecAuthenticationType auth_type) {
  switch (auth_type) {
    case kSecAuthenticationTypeHTMLForm:   return PasswordForm::SCHEME_HTML;
    case kSecAuthenticationTypeHTTPBasic:  return PasswordForm::SCHEME_BASIC;
    case kSecAuthenticationTypeHTTPDigest: return PasswordForm::SCHEME_DIGEST;
    default:                               return PasswordForm::SCHEME_OTHER;
  }
}

bool FillPasswordFormFromKeychainItem(const AppleKeychain& keychain,
                                      const SecKeychainItemRef& keychain_item,
                                      PasswordForm* form,
                                      bool extract_password_data) {
  DCHECK(form);

  SecKeychainAttributeInfo attrInfo;
  UInt32 tags[] = { kSecAccountItemAttr,
                    kSecServerItemAttr,
                    kSecPortItemAttr,
                    kSecPathItemAttr,
                    kSecProtocolItemAttr,
                    kSecAuthenticationTypeItemAttr,
                    kSecSecurityDomainItemAttr,
                    kSecCreationDateItemAttr,
                    kSecNegativeItemAttr };
  attrInfo.count = arraysize(tags);
  attrInfo.tag = tags;
  attrInfo.format = NULL;

  SecKeychainAttributeList* attrList;
  UInt32 password_length;

  // If |extract_password_data| is false, do not pass in a reference to
  // |password_data|. ItemCopyAttributesAndData will then extract only the
  // attributes of |keychain_item| (doesn't require OS authorization), and not
  // attempt to extract its password data (requires OS authorization).
  void* password_data = NULL;
  void** password_data_ref = extract_password_data ? &password_data : NULL;

  OSStatus result = keychain.ItemCopyAttributesAndData(keychain_item, &attrInfo,
                                                       NULL, &attrList,
                                                       &password_length,
                                                       password_data_ref);

  if (result != noErr) {
    // We don't log errSecAuthFailed because that just means that the user
    // chose not to allow us access to the item.
    if (result != errSecAuthFailed) {
      OSSTATUS_LOG(ERROR, result) << "Keychain data load failed";
    }
    return false;
  }

  if (extract_password_data) {
    base::UTF8ToUTF16(static_cast<const char *>(password_data), password_length,
                      &(form->password_value));
  }

  int port = kAnyPort;
  std::string server;
  std::string security_domain;
  std::string path;
  for (unsigned int i = 0; i < attrList->count; i++) {
    SecKeychainAttribute attr = attrList->attr[i];
    if (!attr.data) {
      continue;
    }
    switch (attr.tag) {
      case kSecAccountItemAttr:
        base::UTF8ToUTF16(static_cast<const char *>(attr.data), attr.length,
                          &(form->username_value));
        break;
      case kSecServerItemAttr:
        server.assign(static_cast<const char *>(attr.data), attr.length);
        break;
      case kSecPortItemAttr:
        port = *(static_cast<UInt32*>(attr.data));
        break;
      case kSecPathItemAttr:
        path.assign(static_cast<const char *>(attr.data), attr.length);
        break;
      case kSecProtocolItemAttr:
      {
        SecProtocolType protocol = *(static_cast<SecProtocolType*>(attr.data));
        // TODO(stuartmorgan): Handle proxy types
        form->ssl_valid = (protocol == kSecProtocolTypeHTTPS);
        break;
      }
      case kSecAuthenticationTypeItemAttr:
      {
        SecAuthenticationType auth_type =
            *(static_cast<SecAuthenticationType*>(attr.data));
        form->scheme = SchemeForAuthType(auth_type);
        break;
      }
      case kSecSecurityDomainItemAttr:
        security_domain.assign(static_cast<const char *>(attr.data),
                               attr.length);
        break;
      case kSecCreationDateItemAttr:
        // The only way to get a date out of Keychain is as a string. Really.
        // (The docs claim it's an int, but the header is correct.)
        TimeFromKeychainTimeString(static_cast<char*>(attr.data), attr.length,
                                   &form->date_created);
        break;
      case kSecNegativeItemAttr:
        Boolean negative_item = *(static_cast<Boolean*>(attr.data));
        if (negative_item) {
          form->blacklisted_by_user = true;
        }
        break;
    }
  }
  keychain.ItemFreeAttributesAndData(attrList, password_data);

  // kSecNegativeItemAttr doesn't seem to actually be in widespread use. In
  // practice, other browsers seem to use a "" or " " password (and a special
  // user name) to indicated blacklist entries.
  if (extract_password_data && (form->password_value.empty() ||
                                base::EqualsASCII(form->password_value, " "))) {
    form->blacklisted_by_user = true;
  }

  // Android facet URLs aren't parsed correctly by GURL and need to be handled
  // separately.
  if (password_manager::IsValidAndroidFacetURI(server)) {
    form->signon_realm = server;
    form->origin = GURL();
    form->ssl_valid = true;
  } else {
    form->origin = URLFromComponents(form->ssl_valid, server, port, path);
    // TODO(stuartmorgan): Handle proxies, which need a different signon_realm
    // format.
    form->signon_realm = form->origin.GetOrigin().spec();
    if (form->scheme != PasswordForm::SCHEME_HTML) {
      form->signon_realm.append(security_domain);
    }
  }
  return true;
}

bool HasChromeCreatorCode(const AppleKeychain& keychain,
                          const SecKeychainItemRef& keychain_item) {
  SecKeychainAttributeInfo attr_info;
  UInt32 tags[] = {kSecCreatorItemAttr};
  attr_info.count = arraysize(tags);
  attr_info.tag = tags;
  attr_info.format = nullptr;

  SecKeychainAttributeList* attr_list;
  UInt32 password_length;
  OSStatus result = keychain.ItemCopyAttributesAndData(
      keychain_item, &attr_info, nullptr, &attr_list,
      &password_length, nullptr);
  if (result != noErr)
    return false;
  OSType creator_code = 0;
  for (unsigned int i = 0; i < attr_list->count; i++) {
    SecKeychainAttribute attr = attr_list->attr[i];
    if (!attr.data) {
      continue;
    }
    if (attr.tag == kSecCreatorItemAttr) {
      creator_code = *(static_cast<FourCharCode*>(attr.data));
      break;
    }
  }
  keychain.ItemFreeAttributesAndData(attr_list, nullptr);
  return creator_code && creator_code == base::mac::CreatorCodeForApplication();
}

bool FormsMatchForMerge(const PasswordForm& form_a,
                        const PasswordForm& form_b,
                        FormMatchStrictness strictness) {
  if (IsLoginDatabaseOnlyForm(form_a) || IsLoginDatabaseOnlyForm(form_b))
    return false;

  bool equal_realm = form_a.signon_realm == form_b.signon_realm;
  if (strictness == FUZZY_FORM_MATCH) {
    equal_realm |= form_a.is_public_suffix_match;
  }
  return form_a.scheme == form_b.scheme && equal_realm &&
         form_a.username_value == form_b.username_value;
}

// Moves entries from |forms| that represent either blacklisted or federated
// logins into |extracted|. These two types are stored only in the LoginDatabase
// and do not have corresponding Keychain entries.
void ExtractNonKeychainForms(ScopedVector<autofill::PasswordForm>* forms,
                             ScopedVector<autofill::PasswordForm>* extracted) {
  extracted->reserve(extracted->size() + forms->size());
  ScopedVector<autofill::PasswordForm> remaining;
  MoveAllFormsOut(
      forms, [&remaining, extracted](scoped_ptr<autofill::PasswordForm> form) {
        if (IsLoginDatabaseOnlyForm(*form))
          extracted->push_back(std::move(form));
        else
          remaining.push_back(std::move(form));
      });
  forms->swap(remaining);
}

// Takes |keychain_forms| and |database_forms| and moves the following 2 types
// of forms to |merged_forms|:
//   (1) |database_forms| that by principle never have a corresponding Keychain
//       entry (viz., blacklisted and federated logins),
//   (2) |database_forms| which should have and do have a corresponding entry in
//       |keychain_forms|.
// The database forms of type (2) have their password value updated from the
// corresponding keychain form, and all the keychain forms corresponding to some
// database form are removed from |keychain_forms| and deleted.
void MergePasswordForms(ScopedVector<autofill::PasswordForm>* keychain_forms,
                        ScopedVector<autofill::PasswordForm>* database_forms,
                        ScopedVector<autofill::PasswordForm>* merged_forms) {
  // Pull out the database blacklist items and federated logins, since they are
  // used as-is rather than being merged with keychain forms.
  ExtractNonKeychainForms(database_forms, merged_forms);

  // Merge the normal entries.
  ScopedVector<autofill::PasswordForm> unused_database_forms;
  unused_database_forms.reserve(database_forms->size());
  std::set<const autofill::PasswordForm*> used_keychain_forms;
  // Move all database forms to either |merged_forms| or
  // |unused_database_forms|, based on whether they have a match in the keychain
  // forms or not. If there is a match, add its password to the DB form and
  // mark the keychain form as used.
  MoveAllFormsOut(database_forms, [keychain_forms, &used_keychain_forms,
                                   merged_forms, &unused_database_forms](
                                      scoped_ptr<autofill::PasswordForm> form) {
    const PasswordForm* best_match =
        BestKeychainFormForForm(*form, keychain_forms->get());
    if (best_match) {
      used_keychain_forms.insert(best_match);
      form->password_value = best_match->password_value;
      merged_forms->push_back(std::move(form));
    } else {
      unused_database_forms.push_back(std::move(form));
    }
  });
  database_forms->swap(unused_database_forms);

  // Clear out all the Keychain entries we used.
  ScopedVector<autofill::PasswordForm> unused_keychain_forms;
  unused_keychain_forms.reserve(keychain_forms->size());
  for (auto& keychain_form : *keychain_forms) {
    if (!ContainsKey(used_keychain_forms, keychain_form)) {
      unused_keychain_forms.push_back(keychain_form);
      keychain_form = nullptr;
    }
  }
  keychain_forms->swap(unused_keychain_forms);
}

std::vector<ItemFormPair> ExtractAllKeychainItemAttributesIntoPasswordForms(
    std::vector<SecKeychainItemRef>* keychain_items,
    const AppleKeychain& keychain) {
  DCHECK(keychain_items);
  MacKeychainPasswordFormAdapter keychain_adapter(&keychain);
  *keychain_items = keychain_adapter.GetAllPasswordFormKeychainItems();
  std::vector<ItemFormPair> item_form_pairs;
  for (std::vector<SecKeychainItemRef>::iterator i = keychain_items->begin();
       i != keychain_items->end(); ++i) {
    PasswordForm* form_without_password = new PasswordForm();
    internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        keychain,
        *i,
        form_without_password,
        false);  // Load password attributes, but not password data.
    item_form_pairs.push_back(std::make_pair(&(*i), form_without_password));
  }
  return item_form_pairs;
}

void GetPasswordsForForms(const AppleKeychain& keychain,
                          ScopedVector<autofill::PasswordForm>* database_forms,
                          ScopedVector<autofill::PasswordForm>* passwords) {
  // First load the attributes of all items in the keychain without loading
  // their password data, and then match items in |database_forms| against them.
  // This avoids individually searching through the keychain for passwords
  // matching each form in |database_forms|, and results in a significant
  // performance gain, replacing O(N) keychain search operations with a single
  // operation that loads all keychain items, and then selective reads of only
  // the relevant passwords. See crbug.com/263685.
  std::vector<SecKeychainItemRef> keychain_items;
  std::vector<ItemFormPair> item_form_pairs =
      ExtractAllKeychainItemAttributesIntoPasswordForms(&keychain_items,
                                                        keychain);

  // Next, compare the attributes of the PasswordForms in |database_forms|
  // against those in |item_form_pairs|, and extract password data for each
  // matching PasswordForm using its corresponding SecKeychainItemRef.
  ScopedVector<autofill::PasswordForm> unused_db_forms;
  unused_db_forms.reserve(database_forms->size());
  // Move database forms with a password stored in |keychain| to |passwords|,
  // including the password. The rest is moved to |unused_db_forms|.
  MoveAllFormsOut(database_forms,
                  [&keychain, &item_form_pairs, passwords, &unused_db_forms](
                      scoped_ptr<autofill::PasswordForm> form) {
    ScopedVector<autofill::PasswordForm> keychain_matches =
        ExtractPasswordsMergeableWithForm(keychain, item_form_pairs, *form);

    ScopedVector<autofill::PasswordForm> db_form_container;
    db_form_container.push_back(std::move(form));
    MergePasswordForms(&keychain_matches, &db_form_container, passwords);
    AppendSecondToFirst(&unused_db_forms, &db_form_container);
  });
  database_forms->swap(unused_db_forms);

  STLDeleteContainerPairSecondPointers(item_form_pairs.begin(),
                                       item_form_pairs.end());
  for (SecKeychainItemRef item : keychain_items) {
    keychain.Free(item);
  }
}

// TODO(stuartmorgan): signon_realm for proxies is not yet supported.
bool ExtractSignonRealmComponents(const std::string& signon_realm,
                                  std::string* server,
                                  UInt32* port,
                                  bool* is_secure,
                                  std::string* security_domain) {
  // GURL does not parse Android facet URIs correctly.
  if (password_manager::IsValidAndroidFacetURI(signon_realm)) {
    if (server)
      *server = signon_realm;
    if (is_secure)
      *is_secure = true;
    if (port)
      *port = 0;
    if (security_domain)
      security_domain->clear();
    return true;
  }

  // The signon_realm will be the Origin portion of a URL for an HTML form,
  // and the same but with the security domain as a path for HTTP auth.
  GURL realm_as_url(signon_realm);
  if (!realm_as_url.is_valid()) {
    return false;
  }

  if (server)
    *server = realm_as_url.host();
  if (is_secure)
    *is_secure = realm_as_url.SchemeIsCryptographic();
  if (port)
    *port = realm_as_url.has_port() ? atoi(realm_as_url.port().c_str()) : 0;
  if (security_domain) {
    // Strip the leading '/' off of the path to get the security domain.
    if (realm_as_url.path().length() > 0)
      *security_domain = realm_as_url.path().substr(1);
    else
      security_domain->clear();
  }
  return true;
}

bool FormIsValidAndMatchesOtherForm(const PasswordForm& query_form,
                                    const PasswordForm& other_form) {
  std::string server;
  std::string security_domain;
  UInt32 port;
  bool is_secure;
  if (!ExtractSignonRealmComponents(query_form.signon_realm, &server, &port,
                                    &is_secure, &security_domain)) {
    return false;
  }
  return FormsMatchForMerge(query_form, other_form, STRICT_FORM_MATCH);
}

ScopedVector<autofill::PasswordForm> ExtractPasswordsMergeableWithForm(
    const AppleKeychain& keychain,
    const std::vector<ItemFormPair>& item_form_pairs,
    const PasswordForm& query_form) {
  ScopedVector<autofill::PasswordForm> matches;
  for (std::vector<ItemFormPair>::const_iterator i = item_form_pairs.begin();
       i != item_form_pairs.end(); ++i) {
    if (FormIsValidAndMatchesOtherForm(query_form, *(i->second))) {
      // Create a new object, since the caller is responsible for deleting the
      // returned forms.
      scoped_ptr<PasswordForm> form_with_password(new PasswordForm());
      FillPasswordFormFromKeychainItem(
          keychain, *(i->first), form_with_password.get(),
          true);  // Load password attributes and data.
      // Do not include blacklisted items found in the keychain.
      if (!form_with_password->blacklisted_by_user)
        matches.push_back(std::move(form_with_password));
    }
  }
  return matches;
}

}  // namespace internal_keychain_helpers

#pragma mark -

MacKeychainPasswordFormAdapter::MacKeychainPasswordFormAdapter(
    const AppleKeychain* keychain)
    : keychain_(keychain), finds_only_owned_(false) {
}

ScopedVector<autofill::PasswordForm>
MacKeychainPasswordFormAdapter::PasswordsFillingForm(
    const std::string& signon_realm,
    PasswordForm::Scheme scheme) {
  std::vector<SecKeychainItemRef> keychain_items =
      MatchingKeychainItems(signon_realm, scheme, NULL, NULL);
  return ConvertKeychainItemsToForms(&keychain_items);
}

bool MacKeychainPasswordFormAdapter::HasPasswordExactlyMatchingForm(
    const PasswordForm& query_form) {
  SecKeychainItemRef keychain_item = KeychainItemForForm(query_form);
  if (keychain_item) {
    keychain_->Free(keychain_item);
    return true;
  }
  return false;
}

bool MacKeychainPasswordFormAdapter::HasPasswordsMergeableWithForm(
    const PasswordForm& query_form) {
  if (IsLoginDatabaseOnlyForm(query_form))
    return false;
  std::string username = base::UTF16ToUTF8(query_form.username_value);
  std::vector<SecKeychainItemRef> matches =
      MatchingKeychainItems(query_form.signon_realm, query_form.scheme,
                            NULL, username.c_str());
  for (std::vector<SecKeychainItemRef>::iterator i = matches.begin();
       i != matches.end(); ++i) {
    keychain_->Free(*i);
  }

  return !matches.empty();
}

std::vector<SecKeychainItemRef>
    MacKeychainPasswordFormAdapter::GetAllPasswordFormKeychainItems() {
  SecAuthenticationType supported_auth_types[] = {
    kSecAuthenticationTypeHTMLForm,
    kSecAuthenticationTypeHTTPBasic,
    kSecAuthenticationTypeHTTPDigest,
  };

  std::vector<SecKeychainItemRef> matches;
  for (unsigned int i = 0; i < arraysize(supported_auth_types); ++i) {
    KeychainSearch keychain_search(*keychain_);
    OSType creator = CreatorCodeForSearch();
    keychain_search.Init(NULL,
                         NULL,
                         NULL,
                         &supported_auth_types[i],
                         NULL,
                         NULL,
                         NULL,
                         creator ? &creator : NULL);
    keychain_search.FindMatchingItems(&matches);
  }
  return matches;
}

ScopedVector<autofill::PasswordForm>
MacKeychainPasswordFormAdapter::GetAllPasswordFormPasswords() {
  std::vector<SecKeychainItemRef> items = GetAllPasswordFormKeychainItems();
  return ConvertKeychainItemsToForms(&items);
}

bool MacKeychainPasswordFormAdapter::AddPassword(const PasswordForm& form) {
  // We should never be trying to store a blacklist in the keychain.
  DCHECK(!IsLoginDatabaseOnlyForm(form));

  std::string server;
  std::string security_domain;
  UInt32 port;
  bool is_secure;
  if (!internal_keychain_helpers::ExtractSignonRealmComponents(
           form.signon_realm, &server, &port, &is_secure, &security_domain)) {
    return false;
  }
  std::string path;
  // Path doesn't make sense for Android app credentials.
  if (!password_manager::IsValidAndroidFacetURI(form.signon_realm))
    path = form.origin.path();
  std::string username = base::UTF16ToUTF8(form.username_value);
  std::string password = base::UTF16ToUTF8(form.password_value);
  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecKeychainItemRef new_item = NULL;
  OSStatus result = keychain_->AddInternetPassword(
      NULL, server.size(), server.c_str(),
      security_domain.size(), security_domain.c_str(),
      username.size(), username.c_str(),
      path.size(), path.c_str(),
      port, protocol, AuthTypeForScheme(form.scheme),
      password.size(), password.c_str(), &new_item);

  if (result == noErr) {
    SetKeychainItemCreatorCode(new_item,
                               base::mac::CreatorCodeForApplication());
    keychain_->Free(new_item);
  } else if (result == errSecDuplicateItem) {
    // If we collide with an existing item, find and update it instead.
    SecKeychainItemRef existing_item = KeychainItemForForm(form);
    if (!existing_item) {
      return false;
    }
    bool changed = SetKeychainItemPassword(existing_item, password);
    keychain_->Free(existing_item);
    return changed;
  }

  return result == noErr;
}

bool MacKeychainPasswordFormAdapter::RemovePassword(const PasswordForm& form) {
  SecKeychainItemRef keychain_item = KeychainItemForForm(form);
  if (keychain_item == NULL)
    return false;
  OSStatus result = keychain_->ItemDelete(keychain_item);
  keychain_->Free(keychain_item);
  return result == noErr;
}

void MacKeychainPasswordFormAdapter::SetFindsOnlyOwnedItems(
    bool finds_only_owned) {
  finds_only_owned_ = finds_only_owned;
}

ScopedVector<autofill::PasswordForm>
MacKeychainPasswordFormAdapter::ConvertKeychainItemsToForms(
    std::vector<SecKeychainItemRef>* items) {
  ScopedVector<autofill::PasswordForm> forms;
  for (SecKeychainItemRef item : *items) {
    scoped_ptr<PasswordForm> form(new PasswordForm());
    if (internal_keychain_helpers::FillPasswordFormFromKeychainItem(
            *keychain_, item, form.get(), true)) {
      forms.push_back(std::move(form));
    }
    keychain_->Free(item);
  }
  items->clear();
  return forms;
}

SecKeychainItemRef MacKeychainPasswordFormAdapter::KeychainItemForForm(
    const PasswordForm& form) {
  // We don't store blacklist entries in the keychain, so the answer to "what
  // Keychain item goes with this form" is always "nothing" for blacklists.
  // Same goes for federated logins.
  if (IsLoginDatabaseOnlyForm(form))
    return NULL;

  std::string path;
  // Path doesn't make sense for Android app credentials.
  if (!password_manager::IsValidAndroidFacetURI(form.signon_realm))
    path = form.origin.path();
  std::string username = base::UTF16ToUTF8(form.username_value);
  std::vector<SecKeychainItemRef> matches = MatchingKeychainItems(
      form.signon_realm, form.scheme, path.c_str(), username.c_str());

  if (matches.empty()) {
    return NULL;
  }
  // Free all items after the first, since we won't be returning them.
  for (std::vector<SecKeychainItemRef>::iterator i = matches.begin() + 1;
       i != matches.end(); ++i) {
    keychain_->Free(*i);
  }
  return matches[0];
}

std::vector<SecKeychainItemRef>
    MacKeychainPasswordFormAdapter::MatchingKeychainItems(
        const std::string& signon_realm,
        autofill::PasswordForm::Scheme scheme,
        const char* path, const char* username) {
  std::vector<SecKeychainItemRef> matches;

  std::string server;
  std::string security_domain;
  UInt32 port;
  bool is_secure;
  if (!internal_keychain_helpers::ExtractSignonRealmComponents(
           signon_realm, &server, &port, &is_secure, &security_domain)) {
    // TODO(stuartmorgan): Proxies will currently fail here, since their
    // signon_realm is not a URL. We need to detect the proxy case and handle
    // it specially.
    return matches;
  }
  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecAuthenticationType auth_type = AuthTypeForScheme(scheme);
  const char* auth_domain = (scheme == PasswordForm::SCHEME_HTML) ?
      NULL : security_domain.c_str();
  OSType creator = CreatorCodeForSearch();
  KeychainSearch keychain_search(*keychain_);
  keychain_search.Init(server.c_str(),
                       &port,
                       &protocol,
                       &auth_type,
                       auth_domain,
                       path,
                       username,
                       creator ? &creator : NULL);
  keychain_search.FindMatchingItems(&matches);
  return matches;
}

// Returns the Keychain SecAuthenticationType type corresponding to |scheme|.
SecAuthenticationType MacKeychainPasswordFormAdapter::AuthTypeForScheme(
    PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::SCHEME_HTML:   return kSecAuthenticationTypeHTMLForm;
    case PasswordForm::SCHEME_BASIC:  return kSecAuthenticationTypeHTTPBasic;
    case PasswordForm::SCHEME_DIGEST: return kSecAuthenticationTypeHTTPDigest;
    case PasswordForm::SCHEME_OTHER:  return kSecAuthenticationTypeDefault;
    case PasswordForm::SCHEME_USERNAME_ONLY:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return kSecAuthenticationTypeDefault;
}

bool MacKeychainPasswordFormAdapter::SetKeychainItemPassword(
    const SecKeychainItemRef& keychain_item, const std::string& password) {
  OSStatus result = keychain_->ItemModifyAttributesAndData(keychain_item, NULL,
                                                           password.size(),
                                                           password.c_str());
  return result == noErr;
}

bool MacKeychainPasswordFormAdapter::SetKeychainItemCreatorCode(
    const SecKeychainItemRef& keychain_item, OSType creator_code) {
  SecKeychainAttribute attr = { kSecCreatorItemAttr, sizeof(creator_code),
                                &creator_code };
  SecKeychainAttributeList attrList = { 1, &attr };
  OSStatus result = keychain_->ItemModifyAttributesAndData(keychain_item,
                                                           &attrList, 0, NULL);
  return result == noErr;
}

OSType MacKeychainPasswordFormAdapter::CreatorCodeForSearch() {
  return finds_only_owned_ ? base::mac::CreatorCodeForApplication() : 0;
}

#pragma mark -

PasswordStoreMac::PasswordStoreMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
    scoped_ptr<AppleKeychain> keychain)
    : password_manager::PasswordStore(main_thread_runner, db_thread_runner),
      keychain_(std::move(keychain)),
      login_metadata_db_(nullptr) {
  DCHECK(keychain_);
}

PasswordStoreMac::~PasswordStoreMac() {}

void PasswordStoreMac::InitWithTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner) {
  db_thread_runner_ = background_task_runner;
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
}

PasswordStoreMac::MigrationResult PasswordStoreMac::ImportFromKeychain() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!login_metadata_db_)
    return LOGIN_DB_UNAVAILABLE;

  ScopedVector<PasswordForm> database_forms;
  if (!login_metadata_db_->GetAutofillableLogins(&database_forms))
    return LOGIN_DB_FAILURE;

  ScopedVector<PasswordForm> uninteresting_forms;
  internal_keychain_helpers::ExtractNonKeychainForms(&database_forms,
                                                     &uninteresting_forms);
  // If there are no passwords to lookup in the Keychain then we're done.
  if (database_forms.empty())
    return MIGRATION_OK;

  // Mute the Keychain.
  chrome::ScopedSecKeychainSetUserInteractionAllowed user_interaction_allowed(
      false);

  // Make sure that the encryption key is retrieved from the Keychain so the
  // encryption can be done.
  std::string ciphertext;
  if (!OSCrypt::EncryptString("test", &ciphertext))
    return ENCRYPTOR_FAILURE;

  // Retrieve the passwords.
  // Get all the password attributes first.
  std::vector<SecKeychainItemRef> keychain_items;
  std::vector<internal_keychain_helpers::ItemFormPair> item_form_pairs =
      internal_keychain_helpers::
          ExtractAllKeychainItemAttributesIntoPasswordForms(&keychain_items,
                                                            *keychain_);

  // Next, compare the attributes of the PasswordForms in |database_forms|
  // against those in |item_form_pairs|, and extract password data for each
  // matching PasswordForm using its corresponding SecKeychainItemRef.
  size_t unmerged_forms_count = 0;
  size_t chrome_owned_locked_forms_count = 0;
  for (PasswordForm* form : database_forms) {
    ScopedVector<autofill::PasswordForm> keychain_matches =
        internal_keychain_helpers::ExtractPasswordsMergeableWithForm(
            *keychain_, item_form_pairs, *form);

    const PasswordForm* best_match =
        BestKeychainFormForForm(*form, keychain_matches.get());
    if (best_match) {
      form->password_value = best_match->password_value;
    } else {
      unmerged_forms_count++;
      // Check if any corresponding keychain items are created by Chrome.
      for (const auto& item_form_pair : item_form_pairs) {
        if (internal_keychain_helpers::FormIsValidAndMatchesOtherForm(
                *form, *item_form_pair.second) &&
            internal_keychain_helpers::HasChromeCreatorCode(
                *keychain_, *item_form_pair.first))
          chrome_owned_locked_forms_count++;
      }
    }
  }
  STLDeleteContainerPairSecondPointers(item_form_pairs.begin(),
                                       item_form_pairs.end());
  for (SecKeychainItemRef item : keychain_items)
    keychain_->Free(item);

  if (unmerged_forms_count) {
    UMA_HISTOGRAM_COUNTS(
        "PasswordManager.KeychainMigration.NumPasswordsOnFailure",
        database_forms.size());
    UMA_HISTOGRAM_COUNTS("PasswordManager.KeychainMigration.NumFailedPasswords",
                         unmerged_forms_count);
    UMA_HISTOGRAM_COUNTS(
        "PasswordManager.KeychainMigration.NumChromeOwnedInaccessiblePasswords",
        chrome_owned_locked_forms_count);
    return KEYCHAIN_BLOCKED;
  }
  // Now all the passwords are available. It's time to update LoginDatabase.
  login_metadata_db_->set_clear_password_values(false);
  for (PasswordForm* form : database_forms)
    login_metadata_db_->UpdateLogin(*form);
  return MIGRATION_OK;
}

void PasswordStoreMac::set_login_metadata_db(
    password_manager::LoginDatabase* login_db) {
  login_metadata_db_ = login_db;
  if (login_metadata_db_)
    login_metadata_db_->set_clear_password_values(true);
}

bool PasswordStoreMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare) {
  // The class should be used inside PasswordStoreProxyMac only.
  NOTREACHED();
  return true;
}

void PasswordStoreMac::ReportMetricsImpl(const std::string& sync_username,
                                         bool custom_passphrase_sync_enabled) {
  if (!login_metadata_db_)
    return;
  login_metadata_db_->ReportMetrics(sync_username,
                                    custom_passphrase_sync_enabled);
}

PasswordStoreChangeList PasswordStoreMac::AddLoginImpl(
    const PasswordForm& form) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (login_metadata_db_ && AddToKeychainIfNecessary(form))
    return login_metadata_db_->AddLogin(form);
  return PasswordStoreChangeList();
}

PasswordStoreChangeList PasswordStoreMac::UpdateLoginImpl(
    const PasswordForm& form) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!login_metadata_db_)
    return PasswordStoreChangeList();

  PasswordStoreChangeList changes = login_metadata_db_->UpdateLogin(form);

  MacKeychainPasswordFormAdapter keychain_adapter(keychain_.get());
  if (changes.empty() &&
      !keychain_adapter.HasPasswordsMergeableWithForm(form)) {
    // If the password isn't in either the DB or the keychain, then it must have
    // been deleted after autofill happened, and should not be re-added.
    return changes;
  }

  // The keychain add will update if there is a collision and add if there
  // isn't, which is the behavior we want, so there's no separate update call.
  if (AddToKeychainIfNecessary(form) && changes.empty()) {
    changes = login_metadata_db_->AddLogin(form);
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreMac::RemoveLoginImpl(
    const PasswordForm& form) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  PasswordStoreChangeList changes;
  if (login_metadata_db_ && login_metadata_db_->RemoveLogin(form)) {
    // See if we own a Keychain item associated with this item. We can do an
    // exact search rather than messing around with trying to do fuzzy matching
    // because passwords that we created will always have an exact-match
    // database entry.
    // (If a user does lose their profile but not their keychain we'll treat the
    // entries we find like other imported entries anyway, so it's reasonable to
    // handle deletes on them the way we would for an imported item.)
    MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_.get());
    owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
    if (owned_keychain_adapter.HasPasswordExactlyMatchingForm(form)) {
      // If we don't have other forms using it (i.e., a form differing only by
      // the names of the form elements), delete the keychain entry.
      if (!DatabaseHasFormMatchingKeychainForm(form)) {
        owned_keychain_adapter.RemovePassword(form);
      }
    }

    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreMac::RemoveLoginsByOriginAndTimeImpl(
    const url::Origin& origin,
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  ScopedVector<PasswordForm> forms_to_consider;
  ScopedVector<PasswordForm> forms_to_remove;
  if (login_metadata_db_ &&
      login_metadata_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                  &forms_to_consider)) {
    MoveAllFormsOut(
        &forms_to_consider,
        [this, &origin, &forms_to_remove](
            scoped_ptr<autofill::PasswordForm> form_to_consider) {
          if (origin.IsSameOriginWith(url::Origin(form_to_consider->origin)) &&
              login_metadata_db_->RemoveLogin(*form_to_consider))
            forms_to_remove.push_back(std::move(form_to_consider));
        });
    if (!forms_to_remove.empty()) {
      RemoveKeychainForms(forms_to_remove.get());
      CleanOrphanedForms(&forms_to_remove);  // Add the orphaned forms.
      changes = FormsToRemoveChangeList(forms_to_remove.get());
      LogStatsForBulkDeletion(changes.size());
    }
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreMac::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  ScopedVector<PasswordForm> forms_to_remove;
  if (login_metadata_db_ &&
      login_metadata_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                  &forms_to_remove) &&
      login_metadata_db_->RemoveLoginsCreatedBetween(delete_begin,
                                                     delete_end)) {
    RemoveKeychainForms(forms_to_remove.get());
    CleanOrphanedForms(&forms_to_remove);  // Add the orphaned forms.
    changes = FormsToRemoveChangeList(forms_to_remove.get());
    LogStatsForBulkDeletion(changes.size());
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreMac::RemoveLoginsSyncedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  ScopedVector<PasswordForm> forms_to_remove;
  if (login_metadata_db_ &&
      login_metadata_db_->GetLoginsSyncedBetween(delete_begin, delete_end,
                                                 &forms_to_remove) &&
      login_metadata_db_->RemoveLoginsSyncedBetween(delete_begin, delete_end)) {
    RemoveKeychainForms(forms_to_remove.get());
    CleanOrphanedForms(&forms_to_remove);  // Add the orphaned forms_to_remove.
    changes = FormsToRemoveChangeList(forms_to_remove.get());
    LogStatsForBulkDeletionDuringRollback(changes.size());
  }
  return changes;
}

bool PasswordStoreMac::RemoveStatisticsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  return login_metadata_db_ &&
         login_metadata_db_->stats_table().RemoveStatsBetween(delete_begin,
                                                              delete_end);
}

ScopedVector<autofill::PasswordForm> PasswordStoreMac::FillMatchingLogins(
    const autofill::PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy) {
  chrome::ScopedSecKeychainSetUserInteractionAllowed user_interaction_allowed(
      prompt_policy == ALLOW_PROMPT);

  ScopedVector<PasswordForm> database_forms;
  if (!login_metadata_db_ ||
      !login_metadata_db_->GetLogins(form, &database_forms)) {
    return ScopedVector<autofill::PasswordForm>();
  }

  // Let's gather all signon realms we want to match with keychain entries.
  std::set<std::string> realm_set;
  realm_set.insert(form.signon_realm);
  for (const autofill::PasswordForm* db_form : database_forms) {
    // TODO(vabr): We should not be getting different schemes here.
    // http://crbug.com/340112
    if (form.scheme != db_form->scheme)
      continue;  // Forms with different schemes never match.
    if (db_form->is_public_suffix_match || db_form->is_affiliation_based_match)
      realm_set.insert(db_form->signon_realm);
  }
  ScopedVector<autofill::PasswordForm> keychain_forms;
  for (std::set<std::string>::const_iterator realm = realm_set.begin();
       realm != realm_set.end(); ++realm) {
    MacKeychainPasswordFormAdapter keychain_adapter(keychain_.get());
    ScopedVector<autofill::PasswordForm> temp_keychain_forms =
        keychain_adapter.PasswordsFillingForm(*realm, form.scheme);
    AppendSecondToFirst(&keychain_forms, &temp_keychain_forms);
  }

  ScopedVector<autofill::PasswordForm> matched_forms;
  internal_keychain_helpers::MergePasswordForms(
      &keychain_forms, &database_forms, &matched_forms);

  // Strip any blacklist entries out of the unused Keychain array, then take
  // all the entries that are left (which we can use as imported passwords).
  ScopedVector<PasswordForm> keychain_blacklist_forms;
  internal_keychain_helpers::ExtractNonKeychainForms(&keychain_forms,
                                                     &keychain_blacklist_forms);
  AppendSecondToFirst(&matched_forms, &keychain_forms);

  if (!database_forms.empty()) {
    RemoveDatabaseForms(&database_forms);
    NotifyLoginsChanged(FormsToRemoveChangeList(database_forms.get()));
  }

  return matched_forms;
}

bool PasswordStoreMac::FillAutofillableLogins(
    ScopedVector<PasswordForm>* forms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  forms->clear();

  ScopedVector<PasswordForm> database_forms;
  if (!login_metadata_db_ ||
      !login_metadata_db_->GetAutofillableLogins(&database_forms))
    return false;

  internal_keychain_helpers::GetPasswordsForForms(*keychain_, &database_forms,
                                                  forms);

  if (!database_forms.empty()) {
    RemoveDatabaseForms(&database_forms);
    NotifyLoginsChanged(FormsToRemoveChangeList(database_forms.get()));
  }

  return true;
}

bool PasswordStoreMac::FillBlacklistLogins(ScopedVector<PasswordForm>* forms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_metadata_db_ && login_metadata_db_->GetBlacklistLogins(forms);
}

void PasswordStoreMac::AddSiteStatsImpl(
    const password_manager::InteractionsStats& stats) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (login_metadata_db_)
    login_metadata_db_->stats_table().AddRow(stats);
}

void PasswordStoreMac::RemoveSiteStatsImpl(const GURL& origin_domain) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (login_metadata_db_)
    login_metadata_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<scoped_ptr<password_manager::InteractionsStats>>
PasswordStoreMac::GetSiteStatsImpl(const GURL& origin_domain) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_metadata_db_
             ? login_metadata_db_->stats_table().GetRows(origin_domain)
             : std::vector<scoped_ptr<password_manager::InteractionsStats>>();
}

bool PasswordStoreMac::AddToKeychainIfNecessary(const PasswordForm& form) {
  if (IsLoginDatabaseOnlyForm(form))
    return true;
  MacKeychainPasswordFormAdapter keychainAdapter(keychain_.get());
  return keychainAdapter.AddPassword(form);
}

bool PasswordStoreMac::DatabaseHasFormMatchingKeychainForm(
    const autofill::PasswordForm& form) {
  DCHECK(login_metadata_db_);
  bool has_match = false;
  ScopedVector<autofill::PasswordForm> database_forms;
  if (!login_metadata_db_->GetLogins(form, &database_forms))
    return false;
  for (const autofill::PasswordForm* db_form : database_forms) {
    // Below we filter out fuzzy matched forms because we are only interested
    // in exact ones.
    if (!db_form->is_public_suffix_match &&
        internal_keychain_helpers::FormsMatchForMerge(
            form, *db_form, internal_keychain_helpers::STRICT_FORM_MATCH) &&
        db_form->origin == form.origin) {
      has_match = true;
      break;
    }
  }
  return has_match;
}

void PasswordStoreMac::RemoveDatabaseForms(
  ScopedVector<autofill::PasswordForm>* forms) {
  DCHECK(login_metadata_db_);
  ScopedVector<autofill::PasswordForm> removed_forms;
  MoveAllFormsOut(forms, [this, &removed_forms](
                             scoped_ptr<autofill::PasswordForm> form) {
    if (login_metadata_db_->RemoveLogin(*form))
      removed_forms.push_back(std::move(form));
  });
  removed_forms.swap(*forms);
}

void PasswordStoreMac::RemoveKeychainForms(
    const std::vector<PasswordForm*>& forms) {
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_.get());
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  for (std::vector<PasswordForm*>::const_iterator i = forms.begin();
       i != forms.end(); ++i) {
    owned_keychain_adapter.RemovePassword(**i);
  }
}

void PasswordStoreMac::CleanOrphanedForms(
    ScopedVector<autofill::PasswordForm>* orphaned_forms) {
  DCHECK(orphaned_forms);
  DCHECK(login_metadata_db_);

  ScopedVector<autofill::PasswordForm> database_forms;
  if (!login_metadata_db_->GetAutofillableLogins(&database_forms))
    return;

  // Filter forms with corresponding Keychain entry out of |database_forms|.
  ScopedVector<PasswordForm> forms_with_keychain_entry;
  internal_keychain_helpers::GetPasswordsForForms(*keychain_, &database_forms,
                                                  &forms_with_keychain_entry);

  // Clean up any orphaned database entries.
  RemoveDatabaseForms(&database_forms);

  // Move the orphaned DB forms to the output parameter.
  AppendSecondToFirst(orphaned_forms, &database_forms);
}
