// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PERSON_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PERSON_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace app_list {

// Person holds information about a search result retrieved from the People
// Search Google webservice.
struct Person {
  // Parses the dictionary from the people search result and creates a person
  // object.
  static scoped_ptr<Person> Create(const base::DictionaryValue& dict);

  Person();
  ~Person();

  scoped_ptr<Person> Duplicate();

  // This is a unique id for this person. In the case of a result with an
  // associated Google account, this will always be the same as the owner id.
  // In case of non-Google results, this id is arbitrary but guaranteed to be
  // unique for this person search result.
  std::string id;

  // The Owner Id is a GAIA obfuscated id which can be used to identify a
  // Google contact.
  std::string owner_id;

  // Interaction rank is a number between 0.0-1.0 indicating how frequently
  // you interact with the person.
  double interaction_rank;

  std::string display_name;
  std::string email;
  GURL image_url;

};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PERSON_H_
