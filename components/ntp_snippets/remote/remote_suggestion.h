// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace ntp_snippets {

// Exposed for tests.
extern const int kArticlesRemoteId;
extern const int kChromeReaderDefaultExpiryTimeMins;

class SnippetProto;

class RemoteSuggestion {
 public:
  using PtrVector = std::vector<std::unique_ptr<RemoteSuggestion>>;

  enum ContentType { UNKNOWN, VIDEO };

  ~RemoteSuggestion();

  // Creates a RemoteSuggestion from a dictionary, as returned by Chrome Reader.
  // Returns a null pointer if the dictionary doesn't correspond to a valid
  // suggestion. The keys in the dictionary are expected to be the same as the
  // property name, with exceptions documented in the property comment.
  static std::unique_ptr<RemoteSuggestion> CreateFromChromeReaderDictionary(
      const base::DictionaryValue& dict,
      const base::Time& fetch_date);

  // Creates a RemoteSuggestion from a dictionary, as returned by Chrome Content
  // Suggestions. Returns a null pointer if the dictionary doesn't correspond to
  // a valid suggestion. Maps field names to Chrome Reader field names.
  static std::unique_ptr<RemoteSuggestion>
  CreateFromContentSuggestionsDictionary(const base::DictionaryValue& dict,
                                         int remote_category_id,
                                         const base::Time& fetch_date);

  // Creates an RemoteSuggestion from a protocol buffer. Returns a null pointer
  // if the protocol buffer doesn't correspond to a valid suggestion.
  static std::unique_ptr<RemoteSuggestion> CreateFromProto(
      const SnippetProto& proto);

  // Creates a protocol buffer corresponding to this suggestion, for persisting.
  SnippetProto ToProto() const;

  // Coverts to general content suggestion form
  ContentSuggestion ToContentSuggestion(Category category) const;

  // Returns all ids of the suggestion.
  const std::vector<std::string>& GetAllIDs() const { return ids_; }

  // The unique, primary ID for identifying the suggestion.
  const std::string& id() const { return ids_.front(); }

  // Title of the suggestion.
  const std::string& title() const { return title_; }

  // The main URL pointing to the content web page.
  const GURL& url() const { return url_; }

  // The name of the content's publisher.
  const std::string& publisher_name() const { return publisher_name_; }

  // Link to an AMP version of the content web page, if it exists.
  const GURL& amp_url() const { return amp_url_; }

  // Summary or relevant extract from the content.
  const std::string& snippet() const { return snippet_; }

  // Link to an image representative of the content. Do not fetch this image
  // directly. If initialized by CreateFromChromeReaderDictionary() the relevant
  // key is 'thumbnailUrl'
  const GURL& salient_image_url() const { return salient_image_url_; }

  // When the page pointed by this suggestion was published.  If initialized by
  // CreateFromChromeReaderDictionary() the relevant key is
  // 'creationTimestampSec'
  const base::Time& publish_date() const { return publish_date_; }

  // After this expiration date this suggestion should no longer be presented to
  // the user.
  const base::Time& expiry_date() const { return expiry_date_; }

  // If this suggestion has all the data we need to show a full card to the user
  bool is_complete() const {
    return !id().empty() && !title().empty() && !snippet().empty() &&
           salient_image_url().is_valid() && !publish_date().is_null() &&
           !expiry_date().is_null() && !publisher_name().empty();
  }

  float score() const { return score_; }

  bool should_notify() const { return should_notify_; }
  base::Time notification_deadline() const { return notification_deadline_; }

  ContentType content_type() const { return content_type_; }

  bool is_dismissed() const { return is_dismissed_; }
  void set_dismissed(bool dismissed) { is_dismissed_ = dismissed; }

  // The ID of the remote category this suggestion belongs to, for use with
  // CategoryFactory::FromRemoteCategory.
  int remote_category_id() const { return remote_category_id_; }

  base::Time fetch_date() const { return fetch_date_; }

  // Public for testing.
  static base::Time TimeFromJsonString(const std::string& timestamp_str);
  static std::string TimeToJsonString(const base::Time& time);

 private:
  RemoteSuggestion(const std::vector<std::string>& ids, int remote_category_id);

  // base::MakeUnique doesn't work if the ctor is private.
  static std::unique_ptr<RemoteSuggestion> MakeUnique(
      const std::vector<std::string>& ids,
      int remote_category_id);

  // The first ID in the vector is the primary id.
  std::vector<std::string> ids_;
  std::string title_;
  GURL url_;
  std::string publisher_name_;

  // TODO(mvanouwerkerk): Remove this field and its uses, just use |url_|.
  GURL amp_url_;

  GURL salient_image_url_;
  std::string snippet_;
  base::Time publish_date_;
  base::Time expiry_date_;
  float score_;
  bool is_dismissed_;
  int remote_category_id_;

  bool should_notify_;
  base::Time notification_deadline_;

  ContentType content_type_;

  // The time when the remote suggestion was fetched from the server.
  base::Time fetch_date_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestion);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_H_
