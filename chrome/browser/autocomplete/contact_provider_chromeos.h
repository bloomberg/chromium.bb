// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CONTACT_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CONTACT_PROVIDER_CHROMEOS_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/chromeos/contacts/contact_manager_observer.h"

class AutocompleteInput;

namespace contacts {
class ContactManagerInterface;
}

// AutocompleteProvider implementation that searches through the contacts
// provided by contacts::ContactManager.
class ContactProvider : public AutocompleteProvider,
                        public contacts::ContactManagerObserver {
 public:
  // Key within AutocompleteMatch::additional_info where the corresponding
  // contact's ID is stored.
  static const char kMatchContactIdKey[];

  ContactProvider(
      AutocompleteProviderListener* listener,
      Profile* profile,
      base::WeakPtr<contacts::ContactManagerInterface> contact_manager);

  // AutocompleteProvider overrides:
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  // contacts::ContactManagerObserver overrides:
  virtual void OnContactsUpdated(Profile* profile) OVERRIDE;

 private:
  struct ContactData;
  typedef std::vector<ContactData> ContactDataVector;

  virtual ~ContactProvider();

  // Returns true if |a|'s affinity is greater than |b|'s affinity.
  static bool CompareAffinity(const ContactData& a, const ContactData& b);

  // Updates |contacts_| to match the contacts currently reported by
  // ContactManager.
  void RefreshContacts();

  // Adds an AutocompleteMatch object for |contact| to |matches_| if |contact|
  // is matched by |input|.  |input_words| is |input.text()| split on word
  // boundaries.
  void AddContactIfMatched(const AutocompleteInput& input,
                           const std::vector<string16>& input_words,
                           const ContactData& contact);

  // Returns an AutocompleteMatch object corresponding to the passed-in data.
  AutocompleteMatch CreateAutocompleteMatch(const AutocompleteInput& input,
                                            const ContactData& contact);

  base::WeakPtr<contacts::ContactManagerInterface> contact_manager_;

  // Contacts through which we search, ordered by descending affinity.
  ContactDataVector contacts_;

  DISALLOW_COPY_AND_ASSIGN(ContactProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CONTACT_PROVIDER_CHROMEOS_H_
