// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/contact_provider_chromeos.h"

#include <algorithm>
#include <cmath>

#include "base/i18n/break_iterator.h"
#include "base/i18n/string_search.h"
#include "base/string_split.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace {

// Default affinity assigned to contacts whose |affinity| field is unset.
// TODO(derat): Set this to something reasonable (probably 0.0) once we're
// getting affinity for contacts.
float kDefaultAffinity = 1.0;

// Base match relevance assigned to a contact with an affinity of 0.0.
int kBaseRelevance = 1300;

// Maximum boost to relevance for a contact with an affinity of 1.0.
int kAffinityRelevanceBoost = 200;

// Returns true if |word_to_find| is a prefix of |name_to_search| and marks the
// matching text in |classifications| (which corresponds to the contact's full
// name).  |name_index_in_full_name| contains |name_to_search|'s index within
// the full name or string16::npos if it doesn't appear in it.
bool WordIsNamePrefix(const string16& word_to_find,
                      const string16& name_to_search,
                      size_t name_index_in_full_name,
                      size_t full_name_length,
                      ACMatchClassifications* classifications) {
  DCHECK(classifications);

  size_t match_index = 0;
  size_t match_length = 0;
  if (!base::i18n::StringSearchIgnoringCaseAndAccents(word_to_find,
      name_to_search, &match_index, &match_length) || (match_index != 0))
    return false;

  if (name_index_in_full_name != string16::npos) {
    AutocompleteMatch::ACMatchClassifications new_class;
    AutocompleteMatch::ClassifyLocationInString(name_index_in_full_name,
        match_length, full_name_length, 0, &new_class);
    *classifications = AutocompleteMatch::MergeClassifications(
        *classifications, new_class);
  }

  return true;
}

}  // namespace

// static
const char ContactProvider::kMatchContactIdKey[] = "contact_id";

// Cached information about a contact.
struct ContactProvider::ContactData {
  ContactData(const string16& full_name,
              const string16& given_name,
              const string16& family_name,
              const std::string& contact_id,
              float affinity)
      : full_name(full_name),
        given_name(given_name),
        family_name(family_name),
        given_name_index(string16::npos),
        family_name_index(string16::npos),
        contact_id(contact_id),
        affinity(affinity) {
    base::i18n::StringSearchIgnoringCaseAndAccents(
        given_name, full_name, &given_name_index, NULL);
    base::i18n::StringSearchIgnoringCaseAndAccents(
        family_name, full_name, &family_name_index, NULL);
  }

  string16 full_name;
  string16 given_name;
  string16 family_name;

  // Indices into |full_name| where |given_name| and |family_name| first appear,
  // or string16::npos if they don't appear in it.
  size_t given_name_index;
  size_t family_name_index;

  // Unique ID used to look up additional contact information.
  std::string contact_id;

  // Affinity between the user and this contact, in the range [0.0, 1.0].
  float affinity;
};

ContactProvider::ContactProvider(
    AutocompleteProviderListener* listener,
    Profile* profile,
    base::WeakPtr<contacts::ContactManagerInterface> contact_manager)
    : AutocompleteProvider(listener, profile, TYPE_CONTACT),
      contact_manager_(contact_manager) {
  contact_manager_->AddObserver(this, profile);
  RefreshContacts();
}

void ContactProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  if (minimal_changes)
    return;

  matches_.clear();

  if (input.type() != AutocompleteInput::UNKNOWN &&
      input.type() != AutocompleteInput::QUERY &&
      input.type() != AutocompleteInput::FORCED_QUERY)
    return;

  std::vector<string16> input_words;
  base::i18n::BreakIterator break_iterator(
      input.text(),
      base::i18n::BreakIterator::BREAK_WORD);
  if (break_iterator.Init()) {
    while (break_iterator.Advance()) {
      if (break_iterator.IsWord())
        input_words.push_back(break_iterator.GetString());
    }
  }

  // |contacts_| is ordered by descending affinity.  Since affinity is currently
  // the only signal used for computing relevance, we can stop after we've found
  // kMaxMatches results.
  for (ContactDataVector::const_iterator it = contacts_.begin();
       it != contacts_.end() && matches_.size() < kMaxMatches; ++it)
    AddContactIfMatched(input, input_words, *it);
}

void ContactProvider::OnContactsUpdated(Profile* profile) {
  DCHECK_EQ(profile, profile_);
  RefreshContacts();
}

ContactProvider::~ContactProvider() {
  // Like ContactProvider, ContactManager gets destroyed at profile destruction.
  // Make sure that this class doesn't try to access ContactManager after
  // ContactManager is gone.
  if (contact_manager_.get())
    contact_manager_->RemoveObserver(this, profile_);
}

// static
bool ContactProvider::CompareAffinity(const ContactData& a,
                                      const ContactData& b) {
  return a.affinity > b.affinity;
}

void ContactProvider::RefreshContacts() {
  if (!contact_manager_.get())
    return;

  scoped_ptr<contacts::ContactPointers> contacts =
      contact_manager_->GetAllContacts(profile_);

  contacts_.clear();
  contacts_.reserve(contacts->size());
  for (contacts::ContactPointers::const_iterator it = contacts->begin();
       it != contacts->end(); ++it) {
    const contacts::Contact& contact = **it;
    string16 full_name =
        AutocompleteMatch::SanitizeString(UTF8ToUTF16(contact.full_name()));
    string16 given_name =
        AutocompleteMatch::SanitizeString(UTF8ToUTF16(contact.given_name()));
    string16 family_name =
        AutocompleteMatch::SanitizeString(UTF8ToUTF16(contact.family_name()));
    float affinity =
        contact.has_affinity() ? contact.affinity() : kDefaultAffinity;

    if (!full_name.empty()) {
      contacts_.push_back(
          ContactData(full_name, given_name, family_name, contact.contact_id(),
                      affinity));
    }
  }
  std::sort(contacts_.begin(), contacts_.end(), CompareAffinity);
}

void ContactProvider::AddContactIfMatched(
    const AutocompleteInput& input,
    const std::vector<string16>& input_words,
    const ContactData& contact) {
  // First, check if the whole input string is a prefix of the full name.
  // TODO(derat): Consider additionally segmenting the full name so we can match
  // e.g. middle names or initials even when they aren't typed as a prefix of
  // the full name.
  ACMatchClassifications classifications;
  if (!WordIsNamePrefix(input.text(), contact.full_name, 0,
                        contact.full_name.size(), &classifications)) {
    // If not, check whether every search term is a prefix of the given name
    // or the family name.
    if (input_words.empty())
      return;

    // TODO(derat): Check new matches against previous ones to make sure they
    // don't overlap (e.g. the query "bob b" against a contact with full name
    // "Bob G. Bryson", given name "Bob", and family name "Bryson" should result
    // in classifications "_Bob_ G. _B_ryson" rather than "_Bob_ G. Bryson".
    for (std::vector<string16>::const_iterator it = input_words.begin();
         it != input_words.end(); ++it) {
      if (!WordIsNamePrefix(*it, contact.given_name, contact.given_name_index,
                            contact.full_name.size(), &classifications) &&
          !WordIsNamePrefix(*it, contact.family_name, contact.family_name_index,
                            contact.full_name.size(), &classifications))
        return;
    }
  }

  matches_.push_back(CreateAutocompleteMatch(input, contact));
  matches_.back().contents_class = classifications;
}

AutocompleteMatch ContactProvider::CreateAutocompleteMatch(
    const AutocompleteInput& input,
    const ContactData& contact) {
  AutocompleteMatch match(this, 0, false, AutocompleteMatch::CONTACT);
  match.inline_autocomplete_offset = string16::npos;
  match.contents = contact.full_name;
  match.fill_into_edit = match.contents;
  match.relevance = kBaseRelevance +
      static_cast<int>(roundf(kAffinityRelevanceBoost * contact.affinity));
  match.RecordAdditionalInfo(kMatchContactIdKey, contact.contact_id);
  return match;
}
