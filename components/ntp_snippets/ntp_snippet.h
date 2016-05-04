// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPET_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPET_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace ntp_snippets {

struct SnippetSource {
  SnippetSource(const GURL& url,
                const std::string& publisher_name,
                const GURL& amp_url)
      : url(url), publisher_name(publisher_name), amp_url(amp_url) {}
  GURL url;
  std::string publisher_name;
  GURL amp_url;
};

// Stores and vend fresh content data for the NTP. This is a dumb class with no
// smarts at all, all the logic is in the service.
class NTPSnippet {
 public:
  // Creates a new snippet with the given URL. URL must be valid.
  NTPSnippet(const GURL& url);

  ~NTPSnippet();

  // Creates an NTPSnippet from a dictionary. Returns a null pointer if the
  // dictionary doesn't contain at least a url. The keys in the dictionary are
  // expected to be the same as the property name, with exceptions documented in
  // the property comment.
  static std::unique_ptr<NTPSnippet> CreateFromDictionary(
      const base::DictionaryValue& dict);

  std::unique_ptr<base::DictionaryValue> ToDictionary() const;

  // URL of the page described by this snippet.
  const GURL& url() const { return url_; }

  // Title of the snippet.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // Summary or relevant extract from the content.
  const std::string& snippet() const { return snippet_; }
  void set_snippet(const std::string& snippet) { snippet_ = snippet; }

  // Link to an image representative of the content. Do not fetch this image
  // directly. If initialized by CreateFromDictionary() the relevant key is
  // 'thumbnailUrl'
  const GURL& salient_image_url() const { return salient_image_url_; }
  void set_salient_image_url(const GURL& salient_image_url) {
    salient_image_url_ = salient_image_url;
  }

  // When the page pointed by this snippet was published.  If initialized by
  // CreateFromDictionary() the relevant key is 'creationTimestampSec'
  const base::Time& publish_date() const { return publish_date_; }
  void set_publish_date(const base::Time& publish_date) {
    publish_date_ = publish_date;
  }

  // After this expiration date this snippet should no longer be presented to
  // the user.
  const base::Time& expiry_date() const { return expiry_date_; }
  void set_expiry_date(const base::Time& expiry_date) {
    expiry_date_ = expiry_date;
  }

  size_t source_index() const { return best_source_index_; }
  void set_source_index(size_t index) { best_source_index_ = index; }

  // We should never construct an NTPSnippet object if we don't have any sources
  // so this should never fail
  const SnippetSource& best_source() const {
    return sources_[best_source_index_];
  }

  const std::vector<SnippetSource>& sources() const { return sources_; }
  void add_source(const SnippetSource& source) { sources_.push_back(source); }

  // If this snippet has all the data we need to show a full card to the user
  bool is_complete() const {
    return url().is_valid() && !sources().empty() && !title().empty() &&
           !snippet().empty() && salient_image_url().is_valid() &&
           !publish_date().is_null() && !expiry_date().is_null() &&
           !best_source().publisher_name.empty();
  }

  // Public for testing.
  static base::Time TimeFromJsonString(const std::string& timestamp_str);
  static std::string TimeToJsonString(const base::Time& time);

 private:
  GURL url_;
  std::string title_;
  GURL salient_image_url_;
  std::string snippet_;
  base::Time publish_date_;
  base::Time expiry_date_;
  GURL amp_url_;
  size_t best_source_index_;

  std::vector<SnippetSource> sources_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippet);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPET_H_
