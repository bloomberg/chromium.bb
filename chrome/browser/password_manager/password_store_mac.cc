// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"

#include <CoreServices/CoreServices.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/mac/security_wrappers.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/apple_keychain.h"

using autofill::PasswordForm;
using crypto::AppleKeychain;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;

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

  SecKeychainAttributeList *attrList;
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
                                EqualsASCII(form->password_value, " "))) {
    form->blacklisted_by_user = true;
  }

  form->origin = URLFromComponents(form->ssl_valid, server, port, path);
  // TODO(stuartmorgan): Handle proxies, which need a different signon_realm
  // format.
  form->signon_realm = form->origin.GetOrigin().spec();
  if (form->scheme != PasswordForm::SCHEME_HTML) {
    form->signon_realm.append(security_domain);
  }
  return true;
}

bool FormsMatchForMerge(const PasswordForm& form_a,
                        const PasswordForm& form_b,
                        FormMatchStrictness strictness) {
  // We never merge blacklist entries between our store and the keychain.
  if (form_a.blacklisted_by_user || form_b.blacklisted_by_user) {
    return false;
  }
  bool equal_realm = form_a.signon_realm == form_b.signon_realm;
  if (strictness == FUZZY_FORM_MATCH) {
    equal_realm |= (!form_a.original_signon_realm.empty()) &&
                   form_a.original_signon_realm == form_b.signon_realm;
  }
  return form_a.scheme == form_b.scheme && equal_realm &&
         form_a.username_value == form_b.username_value;
}

// Returns an the best match for |base_form| from |keychain_forms|, or NULL if
// there is no suitable match.
PasswordForm* BestKeychainFormForForm(
    const PasswordForm& base_form,
    const std::vector<PasswordForm*>* keychain_forms) {
  PasswordForm* partial_match = NULL;
  for (std::vector<PasswordForm*>::const_iterator i = keychain_forms->begin();
       i != keychain_forms->end(); ++i) {
    // TODO(stuartmorgan): We should really be scoring path matches and picking
    // the best, rather than just checking exact-or-not (although in practice
    // keychain items with paths probably came from us).
    if (FormsMatchForMerge(base_form, *(*i), FUZZY_FORM_MATCH)) {
      if (base_form.origin == (*i)->origin) {
        return *i;
      } else if (!partial_match) {
        partial_match = *i;
      }
    }
  }
  return partial_match;
}

// Returns entries from |forms| that are blacklist entries, after removing
// them from |forms|.
std::vector<PasswordForm*> ExtractBlacklistForms(
    std::vector<PasswordForm*>* forms) {
  std::vector<PasswordForm*> blacklist_forms;
  for (std::vector<PasswordForm*>::iterator i = forms->begin();
       i != forms->end();) {
    PasswordForm* form = *i;
    if (form->blacklisted_by_user) {
      blacklist_forms.push_back(form);
      i = forms->erase(i);
    } else {
      ++i;
    }
  }
  return blacklist_forms;
}

// Deletes and removes from v any element that exists in s.
template <class T>
void DeleteVectorElementsInSet(std::vector<T*>* v, const std::set<T*>& s) {
  for (typename std::vector<T*>::iterator i = v->begin(); i != v->end();) {
    T* element = *i;
    if (s.find(element) != s.end()) {
      delete element;
      i = v->erase(i);
    } else {
      ++i;
    }
  }
}

void MergePasswordForms(std::vector<PasswordForm*>* keychain_forms,
                        std::vector<PasswordForm*>* database_forms,
                        std::vector<PasswordForm*>* merged_forms) {
  // Pull out the database blacklist items, since they are used as-is rather
  // than being merged with keychain forms.
  std::vector<PasswordForm*> database_blacklist_forms =
      ExtractBlacklistForms(database_forms);

  // Merge the normal entries.
  std::set<PasswordForm*> used_keychain_forms;
  for (std::vector<PasswordForm*>::iterator i = database_forms->begin();
       i != database_forms->end();) {
    PasswordForm* db_form = *i;
    PasswordForm* best_match = BestKeychainFormForForm(*db_form,
                                                       keychain_forms);
    if (best_match) {
      used_keychain_forms.insert(best_match);
      db_form->password_value = best_match->password_value;
      merged_forms->push_back(db_form);
      i = database_forms->erase(i);
    } else {
      ++i;
    }
  }

  // Add in the blacklist entries from the database.
  merged_forms->insert(merged_forms->end(),
                       database_blacklist_forms.begin(),
                       database_blacklist_forms.end());

  // Clear out all the Keychain entries we used.
  DeleteVectorElementsInSet(keychain_forms, used_keychain_forms);
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

std::vector<PasswordForm*> GetPasswordsForForms(
    const AppleKeychain& keychain,
    std::vector<PasswordForm*>* database_forms) {
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
  std::vector<PasswordForm*> merged_forms;
  for (std::vector<PasswordForm*>::iterator i = database_forms->begin();
       i != database_forms->end();) {
    std::vector<PasswordForm*> db_form_container(1, *i);
    std::vector<PasswordForm*> keychain_matches =
        ExtractPasswordsMergeableWithForm(keychain, item_form_pairs, **i);
    MergePasswordForms(&keychain_matches, &db_form_container, &merged_forms);
    if (db_form_container.empty()) {
      i = database_forms->erase(i);
    } else {
      ++i;
    }
    STLDeleteElements(&keychain_matches);
  }

  // Clean up temporary PasswordForms and SecKeychainItemRefs.
  STLDeleteContainerPairSecondPointers(item_form_pairs.begin(),
                                       item_form_pairs.end());
  for (std::vector<SecKeychainItemRef>::iterator i = keychain_items.begin();
       i != keychain_items.end(); ++i) {
    keychain.Free(*i);
  }
  return merged_forms;
}

// TODO(stuartmorgan): signon_realm for proxies is not yet supported.
bool ExtractSignonRealmComponents(const std::string& signon_realm,
                                  std::string* server,
                                  UInt32* port,
                                  bool* is_secure,
                                  std::string* security_domain) {
  // The signon_realm will be the Origin portion of a URL for an HTML form,
  // and the same but with the security domain as a path for HTTP auth.
  GURL realm_as_url(signon_realm);
  if (!realm_as_url.is_valid()) {
    return false;
  }

  if (server)
    *server = realm_as_url.host();
  if (is_secure)
    *is_secure = realm_as_url.SchemeIsSecure();
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
  return internal_keychain_helpers::FormsMatchForMerge(
      query_form, other_form, STRICT_FORM_MATCH);
}

std::vector<PasswordForm*> ExtractPasswordsMergeableWithForm(
    const AppleKeychain& keychain,
    const std::vector<ItemFormPair>& item_form_pairs,
    const PasswordForm& query_form) {
  std::vector<PasswordForm*> matches;
  for (std::vector<ItemFormPair>::const_iterator i = item_form_pairs.begin();
       i != item_form_pairs.end(); ++i) {
    if (FormIsValidAndMatchesOtherForm(query_form, *(i->second))) {
      // Create a new object, since the caller is responsible for deleting the
      // returned forms.
      scoped_ptr<PasswordForm> form_with_password(new PasswordForm());
      internal_keychain_helpers::FillPasswordFormFromKeychainItem(
          keychain,
          *(i->first),
          form_with_password.get(),
          true);  // Load password attributes and data.
      // Do not include blacklisted items found in the keychain.
      if (!form_with_password->blacklisted_by_user)
        matches.push_back(form_with_password.release());
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

std::vector<PasswordForm*> MacKeychainPasswordFormAdapter::PasswordsFillingForm(
    const std::string& signon_realm,
    PasswordForm::Scheme scheme) {
  std::vector<SecKeychainItemRef> keychain_items =
      MatchingKeychainItems(signon_realm, scheme, NULL, NULL);

  return ConvertKeychainItemsToForms(&keychain_items);
}

PasswordForm* MacKeychainPasswordFormAdapter::PasswordExactlyMatchingForm(
    const PasswordForm& query_form) {
  SecKeychainItemRef keychain_item = KeychainItemForForm(query_form);
  if (keychain_item) {
    PasswordForm* form = new PasswordForm();
    internal_keychain_helpers::FillPasswordFormFromKeychainItem(*keychain_,
                                                                keychain_item,
                                                                form,
                                                                true);
    keychain_->Free(keychain_item);
    return form;
  }
  return NULL;
}

bool MacKeychainPasswordFormAdapter::HasPasswordsMergeableWithForm(
    const PasswordForm& query_form) {
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

std::vector<PasswordForm*>
    MacKeychainPasswordFormAdapter::GetAllPasswordFormPasswords() {
  std::vector<SecKeychainItemRef> items = GetAllPasswordFormKeychainItems();
  return ConvertKeychainItemsToForms(&items);
}

bool MacKeychainPasswordFormAdapter::AddPassword(const PasswordForm& form) {
  // We should never be trying to store a blacklist in the keychain.
  DCHECK(!form.blacklisted_by_user);

  std::string server;
  std::string security_domain;
  UInt32 port;
  bool is_secure;
  if (!internal_keychain_helpers::ExtractSignonRealmComponents(
           form.signon_realm, &server, &port, &is_secure, &security_domain)) {
    return false;
  }
  std::string username = base::UTF16ToUTF8(form.username_value);
  std::string password = base::UTF16ToUTF8(form.password_value);
  std::string path = form.origin.path();
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

std::vector<PasswordForm*>
    MacKeychainPasswordFormAdapter::ConvertKeychainItemsToForms(
        std::vector<SecKeychainItemRef>* items) {
  std::vector<PasswordForm*> keychain_forms;
  for (std::vector<SecKeychainItemRef>::const_iterator i = items->begin();
       i != items->end(); ++i) {
    PasswordForm* form = new PasswordForm();
    if (internal_keychain_helpers::FillPasswordFormFromKeychainItem(
            *keychain_, *i, form, true)) {
      keychain_forms.push_back(form);
    }
    keychain_->Free(*i);
  }
  items->clear();
  return keychain_forms;
}

SecKeychainItemRef MacKeychainPasswordFormAdapter::KeychainItemForForm(
    const PasswordForm& form) {
  // We don't store blacklist entries in the keychain, so the answer to "what
  // Keychain item goes with this form" is always "nothing" for blacklists.
  if (form.blacklisted_by_user) {
    return NULL;
  }

  std::string path = form.origin.path();
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
    AppleKeychain* keychain,
    password_manager::LoginDatabase* login_db)
    : password_manager::PasswordStore(main_thread_runner, db_thread_runner),
      keychain_(keychain),
      login_metadata_db_(login_db) {
  DCHECK(keychain_.get());
  DCHECK(login_metadata_db_.get());
}

PasswordStoreMac::~PasswordStoreMac() {}

bool PasswordStoreMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare,
    const std::string& sync_username) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  thread_.reset(new base::Thread("Chrome_PasswordStore_Thread"));

  if (!thread_->Start()) {
    thread_.reset(NULL);
    return false;
  }
  return password_manager::PasswordStore::Init(flare, sync_username);
}

void PasswordStoreMac::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  password_manager::PasswordStore::Shutdown();
  thread_->Stop();
}

// Mac stores passwords in the system keychain, which can block for an
// arbitrarily long time (most notably, it can block on user confirmation
// from a dialog). Run tasks on a dedicated thread to avoid blocking the DB
// thread.
scoped_refptr<base::SingleThreadTaskRunner>
PasswordStoreMac::GetBackgroundTaskRunner() {
  return (thread_.get()) ? thread_->message_loop_proxy() : NULL;
}

void PasswordStoreMac::ReportMetricsImpl(const std::string& sync_username) {
  login_metadata_db_->ReportMetrics(sync_username);
}

PasswordStoreChangeList PasswordStoreMac::AddLoginImpl(
    const PasswordForm& form) {
  DCHECK(thread_->message_loop() == base::MessageLoop::current());
  PasswordStoreChangeList changes;
  if (AddToKeychainIfNecessary(form)) {
    changes = login_metadata_db_->AddLogin(form);
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreMac::UpdateLoginImpl(
    const PasswordForm& form) {
  DCHECK(thread_->message_loop() == base::MessageLoop::current());
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
  DCHECK(thread_->message_loop() == base::MessageLoop::current());
  PasswordStoreChangeList changes;
  if (login_metadata_db_->RemoveLogin(form)) {
    // See if we own a Keychain item associated with this item. We can do an
    // exact search rather than messing around with trying to do fuzzy matching
    // because passwords that we created will always have an exact-match
    // database entry.
    // (If a user does lose their profile but not their keychain we'll treat the
    // entries we find like other imported entries anyway, so it's reasonable to
    // handle deletes on them the way we would for an imported item.)
    MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_.get());
    owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
    PasswordForm* owned_password_form =
        owned_keychain_adapter.PasswordExactlyMatchingForm(form);
    if (owned_password_form) {
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

PasswordStoreChangeList PasswordStoreMac::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  ScopedVector<PasswordForm> forms;
  if (login_metadata_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                  &forms.get())) {
    if (login_metadata_db_->RemoveLoginsCreatedBetween(delete_begin,
                                                       delete_end)) {
      RemoveKeychainForms(forms.get());

      for (std::vector<PasswordForm*>::const_iterator it = forms.begin();
           it != forms.end(); ++it) {
        changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                              **it));
      }
      LogStatsForBulkDeletion(changes.size());
    }
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreMac::RemoveLoginsSyncedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  ScopedVector<PasswordForm> forms;
  if (login_metadata_db_->GetLoginsSyncedBetween(
          delete_begin, delete_end, &forms.get())) {
    if (login_metadata_db_->RemoveLoginsSyncedBetween(delete_begin,
                                                      delete_end)) {
      RemoveKeychainForms(forms.get());

      for (std::vector<PasswordForm*>::const_iterator it = forms.begin();
           it != forms.end();
           ++it) {
        changes.push_back(
            PasswordStoreChange(PasswordStoreChange::REMOVE, **it));
      }
    }
  }
  return changes;
}

void PasswordStoreMac::GetLoginsImpl(
    const autofill::PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy,
    const ConsumerCallbackRunner& callback_runner) {
  chrome::ScopedSecKeychainSetUserInteractionAllowed user_interaction_allowed(
      prompt_policy == ALLOW_PROMPT);

  std::vector<PasswordForm*> database_forms;
  login_metadata_db_->GetLogins(form, &database_forms);

  // Let's gather all signon realms we want to match with keychain entries.
  std::set<std::string> realm_set;
  realm_set.insert(form.signon_realm);
  for (std::vector<PasswordForm*>::const_iterator db_form =
           database_forms.begin();
       db_form != database_forms.end();
       ++db_form) {
    // TODO(vabr): We should not be getting different schemes here.
    // http://crbug.com/340112
    if (form.scheme != (*db_form)->scheme)
      continue;  // Forms with different schemes never match.
    const std::string& original_singon_realm((*db_form)->original_signon_realm);
    if (!original_singon_realm.empty())
      realm_set.insert(original_singon_realm);
  }
  std::vector<PasswordForm*> keychain_forms;
  for (std::set<std::string>::const_iterator realm = realm_set.begin();
       realm != realm_set.end();
       ++realm) {
    MacKeychainPasswordFormAdapter keychain_adapter(keychain_.get());
    std::vector<PasswordForm*> temp_keychain_forms =
        keychain_adapter.PasswordsFillingForm(*realm, form.scheme);
    keychain_forms.insert(keychain_forms.end(),
                          temp_keychain_forms.begin(),
                          temp_keychain_forms.end());
  }

  std::vector<PasswordForm*> matched_forms;
  internal_keychain_helpers::MergePasswordForms(&keychain_forms,
                                                &database_forms,
                                                &matched_forms);

  // Strip any blacklist entries out of the unused Keychain array, then take
  // all the entries that are left (which we can use as imported passwords).
  std::vector<PasswordForm*> keychain_blacklist_forms =
      internal_keychain_helpers::ExtractBlacklistForms(&keychain_forms);
  matched_forms.insert(matched_forms.end(),
                       keychain_forms.begin(),
                       keychain_forms.end());
  keychain_forms.clear();
  STLDeleteElements(&keychain_blacklist_forms);

  // Clean up any orphaned database entries.
  RemoveDatabaseForms(database_forms);
  STLDeleteElements(&database_forms);

  callback_runner.Run(matched_forms);
}

void PasswordStoreMac::GetBlacklistLoginsImpl(GetLoginsRequest* request) {
  FillBlacklistLogins(request->result());
  ForwardLoginsResult(request);
}

void PasswordStoreMac::GetAutofillableLoginsImpl(GetLoginsRequest* request) {
  FillAutofillableLogins(request->result());
  ForwardLoginsResult(request);
}

bool PasswordStoreMac::FillAutofillableLogins(
         std::vector<PasswordForm*>* forms) {
  DCHECK(thread_->message_loop() == base::MessageLoop::current());

  std::vector<PasswordForm*> database_forms;
  login_metadata_db_->GetAutofillableLogins(&database_forms);

  std::vector<PasswordForm*> merged_forms =
      internal_keychain_helpers::GetPasswordsForForms(*keychain_,
                                                      &database_forms);

  // Clean up any orphaned database entries.
  RemoveDatabaseForms(database_forms);
  STLDeleteElements(&database_forms);

  forms->insert(forms->end(), merged_forms.begin(), merged_forms.end());
  return true;
}

bool PasswordStoreMac::FillBlacklistLogins(
         std::vector<PasswordForm*>* forms) {
  DCHECK(thread_->message_loop() == base::MessageLoop::current());
  return login_metadata_db_->GetBlacklistLogins(forms);
}

bool PasswordStoreMac::AddToKeychainIfNecessary(const PasswordForm& form) {
  if (form.blacklisted_by_user) {
    return true;
  }
  MacKeychainPasswordFormAdapter keychainAdapter(keychain_.get());
  return keychainAdapter.AddPassword(form);
}

bool PasswordStoreMac::DatabaseHasFormMatchingKeychainForm(
    const autofill::PasswordForm& form) {
  bool has_match = false;
  std::vector<PasswordForm*> database_forms;
  login_metadata_db_->GetLogins(form, &database_forms);
  for (std::vector<PasswordForm*>::iterator i = database_forms.begin();
       i != database_forms.end(); ++i) {
    // Below we filter out forms with non-empty original_signon_realm, because
    // those signal fuzzy matches, and we are only interested in exact ones.
    if ((*i)->original_signon_realm.empty() &&
        internal_keychain_helpers::FormsMatchForMerge(
            form, **i, internal_keychain_helpers::STRICT_FORM_MATCH) &&
        (*i)->origin == form.origin) {
      has_match = true;
      break;
    }
  }
  STLDeleteElements(&database_forms);
  return has_match;
}

void PasswordStoreMac::RemoveDatabaseForms(
    const std::vector<PasswordForm*>& forms) {
  for (std::vector<PasswordForm*>::const_iterator i = forms.begin();
       i != forms.end(); ++i) {
    login_metadata_db_->RemoveLogin(**i);
  }
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
