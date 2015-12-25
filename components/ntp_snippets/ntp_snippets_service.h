// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/inner_iterator.h"
#include "components/ntp_snippets/ntp_snippet.h"

namespace ntp_snippets {

class NTPSnippetsServiceObserver;

// Stores and vend fresh content data for the NTP.
class NTPSnippetsService : public KeyedService {
 public:
  using NTPSnippetStorage = std::vector<std::unique_ptr<NTPSnippet>>;
  using const_iterator =
      InnerIterator<NTPSnippetStorage::const_iterator, const NTPSnippet>;

  // |application_language_code| should be a ISO 639-1 compliant string. Aka
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (english language with US locale) and 'en-GB_US'
  // (British english person in the US) are not language code.
  explicit NTPSnippetsService(const std::string& application_language_code);
  ~NTPSnippetsService() override;

  // Inherited from KeyedService.
  void Shutdown() override;

  // True once the data is loaded in memory and available to loop over.
  bool is_loaded() { return loaded_; }

  // Observer accessors.
  void AddObserver(NTPSnippetsServiceObserver* observer);
  void RemoveObserver(NTPSnippetsServiceObserver* observer);

  // Expects the JSON to be a list of dictionaries with keys matching the
  // properties of a snippet (url, title, site_title, etc...). The url is the
  // only mandatory value.
  bool LoadFromJSONString(const std::string& str);

  // Number of snippets available. Can only be called when is_loaded() is true.
  NTPSnippetStorage::size_type size() {
    DCHECK(loaded_);
    return snippets_.size();
  }

  // The snippets can be iterated upon only via a const_iterator. Recommended
  // way to iterate is as follow:
  //
  // NTPSnippetsService service; // Assume is set.
  //  for (auto& snippet : *service) {
  //    // Snippet here is a const object.
  //  }
  const_iterator begin() {
    DCHECK(loaded_);
    return const_iterator(snippets_.begin());
  }
  const_iterator end() {
    DCHECK(loaded_);
    return const_iterator(snippets_.end());
  }

 private:
  // True if the suggestions are loaded.
  bool loaded_;
  // All the suggestions.
  NTPSnippetStorage snippets_;
  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;
  // The observers.
  base::ObserverList<NTPSnippetsServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsService);
};

class NTPSnippetsServiceObserver {
 public:
  // Send everytime the service loads a new set of data.
  virtual void NTPSnippetsServiceLoaded(NTPSnippetsService* service) = 0;
  // Send when the service is shutting down.
  virtual void NTPSnippetsServiceShutdown(NTPSnippetsService* service) = 0;

 protected:
  virtual ~NTPSnippetsServiceObserver() {}
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
