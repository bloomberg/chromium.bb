// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/search_suggestion_parser.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

scoped_ptr<base::DictionaryValue> AsDictionary(const std::string& json) {
  base::Value* value = base::JSONReader::Read(json);
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict))
    return scoped_ptr<base::DictionaryValue>(dict);

  delete value;
  return scoped_ptr<base::DictionaryValue>(new base::DictionaryValue);
}

}  // namespace

TEST(SearchSuggestionParser, GetAnswersImageURLsWithoutImagelines) {
  std::vector<GURL> urls;

  // No "l" entry in the dictionary.
  SearchSuggestionParser::GetAnswersImageURLs(AsDictionary("").get(), &urls);
  EXPECT_TRUE(urls.empty());

  // Empty "l" entry in the dictionary.
  SearchSuggestionParser::GetAnswersImageURLs(
      AsDictionary("{ \"l\" : {} } ").get(), &urls);
  EXPECT_TRUE(urls.empty());
}

TEST(SearchSuggestionParser, GetAnswersImageURLsWithValidImage) {
  std::vector<GURL> urls;

  const char answer_json[] =
      "{ \"l\" : [{\"il\": { \"i\": {\"d\": "
      "\"//ssl.gstatic.com/foo.png\",\"t\": 3}}}]}";
  SearchSuggestionParser::GetAnswersImageURLs(AsDictionary(answer_json).get(),
                                              &urls);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ("https://ssl.gstatic.com/foo.png", urls[0].spec());
}
