// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

// Basic functionality for the SearchHostToURLsMap tests.
class SearchHostToURLsMapTest : public testing::Test {
 public:
  SearchHostToURLsMapTest() {}

  virtual void SetUp();
  virtual void TearDown() {
    TemplateURLRef::SetGoogleBaseURL(NULL);
  }

 protected:
  void SetGoogleBaseURL(const std::string& base_url) const {
    TemplateURLRef::SetGoogleBaseURL(new std::string(base_url));
  }

  scoped_ptr<SearchHostToURLsMap> provider_map_;
  TemplateURL t_urls_[2];
  std::string host_;

  DISALLOW_COPY_AND_ASSIGN(SearchHostToURLsMapTest);
};

void SearchHostToURLsMapTest::SetUp() {
  // Add some entries to the search host map.
  host_ = "www.unittest.com";
  t_urls_[0].SetURL("http://" + host_ + "/path1", 0, 0);
  t_urls_[1].SetURL("http://" + host_ + "/path2", 0, 0);

  std::vector<const TemplateURL*> template_urls;
  template_urls.push_back(&t_urls_[0]);
  template_urls.push_back(&t_urls_[1]);

  provider_map_.reset(new SearchHostToURLsMap);
  UIThreadSearchTermsData search_terms_data;
  provider_map_->Init(template_urls, search_terms_data);
}

TEST_F(SearchHostToURLsMapTest, Add) {
  std::string new_host = "example.com";
  TemplateURL new_t_url;
  new_t_url.SetURL("http://" + new_host + "/", 0, 0);
  UIThreadSearchTermsData search_terms_data;
  provider_map_->Add(&new_t_url, search_terms_data);

  ASSERT_EQ(&new_t_url, provider_map_->GetTemplateURLForHost(new_host));
}

TEST_F(SearchHostToURLsMapTest, Remove) {
  provider_map_->Remove(&t_urls_[0]);

  const TemplateURL* found_url = provider_map_->GetTemplateURLForHost(host_);
  ASSERT_TRUE(found_url == &t_urls_[1]);

  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host_);
  ASSERT_TRUE(urls != NULL);

  int url_count = 0;
  for (TemplateURLSet::const_iterator i = urls->begin();
       i != urls->end(); ++i) {
    url_count++;
    ASSERT_TRUE(*i == &t_urls_[1]);
  }
  ASSERT_EQ(1, url_count);
}

TEST_F(SearchHostToURLsMapTest, Update) {
  std::string new_host = "example.com";
  TemplateURL new_values;
  new_values.SetURL("http://" + new_host + "/", 0, 0);

  UIThreadSearchTermsData search_terms_data;
  provider_map_->Update(&t_urls_[0], new_values, search_terms_data);

  ASSERT_EQ(&t_urls_[0], provider_map_->GetTemplateURLForHost(new_host));
  ASSERT_EQ(&t_urls_[1], provider_map_->GetTemplateURLForHost(host_));
}

TEST_F(SearchHostToURLsMapTest, UpdateGoogleBaseURLs) {
  UIThreadSearchTermsData search_terms_data;
  std::string google_base_url = "google.com";
  SetGoogleBaseURL("http://" + google_base_url +"/");

  // Add in a url with the templated Google base url.
  TemplateURL new_t_url;
  new_t_url.SetURL("{google:baseURL}?q={searchTerms}", 0, 0);
  provider_map_->Add(&new_t_url, search_terms_data);
  ASSERT_EQ(&new_t_url, provider_map_->GetTemplateURLForHost(google_base_url));

  // Now change the Google base url and verify the result.
  std::string new_google_base_url = "other.com";
  SetGoogleBaseURL("http://" + new_google_base_url +"/");
  provider_map_->UpdateGoogleBaseURLs(search_terms_data);
  ASSERT_EQ(&new_t_url, provider_map_->GetTemplateURLForHost(
      new_google_base_url));
}

TEST_F(SearchHostToURLsMapTest, GetTemplateURLForKnownHost) {
  const TemplateURL* found_url = provider_map_->GetTemplateURLForHost(host_);
  ASSERT_TRUE(found_url == &t_urls_[0] || found_url == &t_urls_[1]);
}

TEST_F(SearchHostToURLsMapTest, GetTemplateURLForUnknownHost) {
  const TemplateURL* found_url = provider_map_->GetTemplateURLForHost(
      "a" + host_);
  ASSERT_TRUE(found_url == NULL);
}

TEST_F(SearchHostToURLsMapTest, GetURLsForKnownHost) {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host_);
  ASSERT_TRUE(urls != NULL);

  bool found_urls[arraysize(t_urls_)] = { 0 };

  for (TemplateURLSet::const_iterator i = urls->begin();
       i != urls->end(); ++i) {
    const TemplateURL* url = *i;
    for (size_t i = 0; i < arraysize(found_urls); ++i) {
      if (url == &t_urls_[i]) {
        found_urls[i] = true;
        break;
      }
    }
  }

  for (size_t i = 0; i < arraysize(found_urls); ++i)
    ASSERT_TRUE(found_urls[i]);
}

TEST_F(SearchHostToURLsMapTest, GetURLsForUnknownHost) {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost("a" + host_);
  ASSERT_TRUE(urls == NULL);
}
