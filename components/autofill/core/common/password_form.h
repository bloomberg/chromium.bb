// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "url/gurl.h"

namespace autofill {

// The PasswordForm struct encapsulates information about a login form,
// which can be an HTML form or a dialog with username/password text fields.
//
// The Web Data database stores saved username/passwords and associated form
// metdata using a PasswordForm struct, typically one that was created from
// a parsed HTMLFormElement or LoginDialog, but the saved entries could have
// also been created by imported data from another browser.
//
// The PasswordManager implements a fuzzy-matching algorithm to compare saved
// PasswordForm entries against PasswordForms that were created from a parsed
// HTML or dialog form. As one might expect, the more data contained in one
// of the saved PasswordForms, the better the job the PasswordManager can do
// in matching it against the actual form it was saved on, and autofill
// accurately. But it is not always possible, especially when importing from
// other browsers with different data models, to copy over all the information
// about a particular "saved password entry" to our PasswordForm
// representation.
//
// The field descriptions in the struct specification below are intended to
// describe which fields are not strictly required when adding a saved password
// entry to the database and how they can affect the matching process.

struct PasswordForm {
  // Enum to differentiate between HTML form based authentication, and dialogs
  // using basic or digest schemes. Default is SCHEME_HTML. Only PasswordForms
  // of the same Scheme will be matched/autofilled against each other.
  enum Scheme {
    SCHEME_HTML,
    SCHEME_BASIC,
    SCHEME_DIGEST,
    SCHEME_OTHER,
    SCHEME_LAST = SCHEME_OTHER
  } scheme;

  // The "Realm" for the sign-on (scheme, host, port for SCHEME_HTML, and
  // contains the HTTP realm for dialog-based forms).
  // The signon_realm is effectively the primary key used for retrieving
  // data from the database, so it must not be empty.
  std::string signon_realm;

  // The original "Realm" for the sign-on (scheme, host, port for SCHEME_HTML,
  // and contains the HTTP realm for dialog-based forms). This realm is only set
  // when two PasswordForms are matched when trying to find a login/pass pair
  // for a site. It is only set to a non-empty value during a match of the
  // original stored login/pass and the current observed form if all these
  // statements are true:
  // 1) The full signon_realm is not the same.
  // 2) The registry controlled domain is the same. For example; example.com,
  // m.example.com, foo.login.example.com and www.example.com would all resolve
  // to example.com since .com is the public suffix.
  // 3) The scheme is the same.
  // 4) The port is the same.
  // For example, if there exists a stored password for http://www.example.com
  // (where .com is the public suffix) and the observed form is
  // http://m.example.com, |original_signon_realm| must be set to
  // http://www.example.com.
  std::string original_signon_realm;

  // The URL (minus query parameters) containing the form. This is the primary
  // data used by the PasswordManager to decide (in longest matching prefix
  // fashion) whether or not a given PasswordForm result from the database is a
  // good fit for a particular form on a page, so it must not be empty.
  GURL origin;

  // The action target of the form. This is the primary data used by the
  // PasswordManager for form autofill; that is, the action of the saved
  // credentials must match the action of the form on the page to be autofilled.
  // If this is empty / not available, it will result in a "restricted"
  // IE-like autofill policy, where we wait for the user to type in his
  // username before autofilling the password. In these cases, after successful
  // login the action URL will automatically be assigned by the
  // PasswordManager.
  //
  // When parsing an HTML form, this must always be set.
  GURL action;

  // The name of the submit button used. Optional; only used in scoring
  // of PasswordForm results from the database to make matches as tight as
  // possible.
  //
  // When parsing an HTML form, this must always be set.
  base::string16 submit_element;

  // The name of the username input element. Optional (improves scoring).
  //
  // When parsing an HTML form, this must always be set.
  base::string16 username_element;

  // The username. Optional.
  //
  // When parsing an HTML form, this is typically empty unless the site
  // has implemented some form of autofill.
  base::string16 username_value;

  // This member is populated in cases where we there are multiple input
  // elements that could possibly be the username. Used when our heuristics for
  // determining the username are incorrect. Optional.
  //
  // When parsing an HTML form, this is typically empty.
  std::vector<base::string16> other_possible_usernames;

  // The name of the input element corresponding to the current password.
  // Optional (improves scoring).
  //
  // When parsing an HTML form, this will always be set, unless it is a sign-up
  // form or a change password form that does not ask for the current password.
  // In these two cases the |new_password_element| will always be set.
  base::string16 password_element;

  // The current password. Must be non-empty for PasswordForm instances that are
  // meant to be persisted to the password store.
  //
  // When parsing an HTML form, this is typically empty.
  base::string16 password_value;

  // False if autocomplete is set to "off" for the password input element;
  // True otherwise.
  bool password_autocomplete_set;

  // If the form was a sign-up or a change password form, the name of the input
  // element corresponding to the new password. Optional, and not persisted.
  base::string16 new_password_element;

  // The new password. Optional, and not persisted.
  base::string16 new_password_value;

  // Whether or not this login was saved under an HTTPS session with a valid
  // SSL cert. We will never match or autofill a PasswordForm where
  // ssl_valid == true with a PasswordForm where ssl_valid == false. This means
  // passwords saved under HTTPS will never get autofilled onto an HTTP page.
  // When importing, this should be set to true if the page URL is HTTPS, thus
  // giving it "the benefit of the doubt" that the SSL cert was valid when it
  // was saved. Default to false.
  bool ssl_valid;

  // True if this PasswordForm represents the last username/password login the
  // user selected to log in to the site. If there is only one saved entry for
  // the site, this will always be true, but when there are multiple entries
  // the PasswordManager ensures that only one of them has a preferred bit set
  // to true. Default to false.
  //
  // When parsing an HTML form, this is not used.
  bool preferred;

  // When the login was saved (by chrome).
  //
  // When parsing an HTML form, this is not used.
  base::Time date_created;

  // When the login was downloaded from the sync server. For local passwords is
  // not used.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_synced;

  // Tracks if the user opted to never remember passwords for this form. Default
  // to false.
  //
  // When parsing an HTML form, this is not used.
  bool blacklisted_by_user;

  // Enum to differentiate between manually filled forms and forms with auto
  // generated passwords.
  enum Type {
    TYPE_MANUAL,
    TYPE_GENERATED,
    TYPE_LAST = TYPE_GENERATED
  };

  // The form type. Not used yet. Please see http://crbug.com/152422
  Type type;

  // The number of times that this username/password has been used to
  // authenticate the user.
  //
  // When parsing an HTML form, this is not used.
  int times_used;

  // True if additional system level authentication should be used
  // (if available) before using this password for autofill.
  //
  // Default to false.
  bool use_additional_authentication;

  // Autofill representation of this form. Used to communicate with the
  // Autofill servers if necessary. Currently this is only used to help
  // determine forms where we can trigger password generation.
  //
  // When parsing an HTML form, this is normally set.
  FormData form_data;

  // Returns true if this match was found using public suffix matching.
  bool IsPublicSuffixMatch() const;

  // Equality operators for testing.
  bool operator==(const PasswordForm& form) const;
  bool operator!=(const PasswordForm& form) const;

  PasswordForm();
  ~PasswordForm();
};

// Map username to PasswordForm* for convenience. See password_form_manager.h.
typedef std::map<base::string16, PasswordForm*> PasswordFormMap;

typedef std::map<base::string16, const PasswordForm*> ConstPasswordFormMap;

// For testing.
std::ostream& operator<<(std::ostream& os,
                         const autofill::PasswordForm& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__
