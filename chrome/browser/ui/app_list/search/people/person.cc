// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/people/person.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace {

const char kKeyId[] = "person.id";
const char kKeyNames[] = "person.names";
const char kKeyDisplayName[] = "displayName";
const char kKeyEmails[] = "person.emails";
const char kKeyEmailValue[] = "value";
const char kKeyInteractionRank[] = "person.sortKeys.interactionRank";
const char kKeyImages[] = "person.images";
const char kKeyUrl[] = "url";
const char kKeyOwnerId[] = "person.metadata.ownerId";

// Finds a list in a dictionary, specified by list_key, then returns the
// first value associated with item_key in the first dictionary in that list.
// So, for this dictionary,
// { key_random: value1,
//   list_key: [ { random_key: value2,
//                 item_key: TARGET_VALUE,
//                 another_random_key: value3 } ],
//   another_key: value4 }
//
//  we'll return TARGET_VALUE.
//
// The reason for this (seemingly) strange behavior is that several of our
// results are going to be in this form and we need to repeat this operation
// to parse them.
std::string GetTargetValue(const base::DictionaryValue& dict,
                           const char list_key[],
                           const char item_key[]) {
  const base::ListValue* list;
  if (!dict.GetList(list_key, &list) || !list)
    return std::string();

  base::ListValue::const_iterator it = list->begin();
  if (it == list->end())
    return std::string();

  base::DictionaryValue* sub_dict;
  if (!(*it)->GetAsDictionary(&sub_dict) || !sub_dict)
    return std::string();

  std::string value;
  if (!sub_dict->GetString(item_key, &value))
    return std::string();

  return value;
}

}  // namespace


namespace app_list {

// static
scoped_ptr<Person> Person::Create(const base::DictionaryValue& dict) {
  scoped_ptr<Person> person(new Person());

  // Person id's.
  if (!dict.GetString(kKeyId, &person->id) ||
      !dict.GetString(kKeyOwnerId, &person->owner_id)) {
    person.reset();
    return person.Pass();
  }

  // Interaction rank.
  std::string interaction_rank_string;
  if (!dict.GetString(kKeyInteractionRank, &interaction_rank_string) ||
      !base::StringToDouble(
            interaction_rank_string, &person->interaction_rank)) {
    person.reset();
    return person.Pass();
  }

  person->display_name = GetTargetValue(dict, kKeyNames, kKeyDisplayName);
  person->email = GetTargetValue(dict, kKeyEmails, kKeyEmailValue);
  person->image_url = GURL(GetTargetValue(dict, kKeyImages, kKeyUrl));

  // If any of our values are invalid, null out our result.
  if (person->id.empty() ||
      person->owner_id.empty() ||
      person->display_name.empty() ||
      person->email.empty() ||
      !person->image_url.is_valid() ||
      person->interaction_rank == 0.0) {
    person.reset();
  }

  return person.Pass();
}

Person::Person() : interaction_rank(0.0) {
}

Person::~Person() {
}

scoped_ptr<Person> Person::Duplicate() {
  scoped_ptr<Person> person(new Person());
  *person = *this;
  return person.Pass();
}

}  // namespace app_list
