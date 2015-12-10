// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_MATCHER_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_MATCHER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"

namespace re2 {
class RE2;
}

class GURL;

namespace chrome {

namespace android {

// DataUseMatcher stores the matching URL patterns and package names along with
// the labels. It also provides functionality to get the matching label for a
// given URL or package. DataUseMatcher is not thread safe.
class DataUseMatcher {
 public:
  explicit DataUseMatcher(
      const base::WeakPtr<DataUseTabModel>& data_use_tab_model);

  ~DataUseMatcher();

  // Called by FetchMatchingRulesDoneOnIOThread to register multiple
  // case-insensitive regular expressions. If the url of the data use request
  // matches any of the regular expression, the observation is passed to the
  // Java listener. All vectors must be non-null and are owned by the caller.
  void RegisterURLRegexes(const std::vector<std::string>* app_package_name,
                          const std::vector<std::string>* domain_path_regex,
                          const std::vector<std::string>* label);

  // Returns true if the |url| matches the registered regular expressions.
  // |label| must not be null. If a match is found, the |label| is set to the
  // matching rule's label.
  bool MatchesURL(const GURL& url, std::string* label) const WARN_UNUSED_RESULT;

  // Returns true if the |app_package_name| matches the registered package
  // names. |label| must not be null. If a match is found, the |label| is set
  // to the matching rule's label.
  bool MatchesAppPackageName(const std::string& app_package_name,
                             std::string* label) const WARN_UNUSED_RESULT;

 private:
  // Stores the matching rules.
  class MatchingRule {
   public:
    MatchingRule(const std::string& app_package_name,
                 scoped_ptr<re2::RE2> pattern,
                 const std::string& label);
    ~MatchingRule();

    const re2::RE2* pattern() const;
    const std::string& app_package_name() const;
    const std::string& label() const;

   private:
    // Package name of the app that should be matched.
    const std::string app_package_name_;

    // RE2 pattern to match against URLs.
    scoped_ptr<re2::RE2> pattern_;

    // Opaque label that uniquely identifies this matching rule.
    const std::string label_;

    DISALLOW_COPY_AND_ASSIGN(MatchingRule);
  };

  base::ThreadChecker thread_checker_;

  std::vector<scoped_ptr<MatchingRule>> matching_rules_;

  // |data_use_tab_model_| is notified if a label is removed from the set of
  // matching labels.
  base::WeakPtr<DataUseTabModel> data_use_tab_model_;

  DISALLOW_COPY_AND_ASSIGN(DataUseMatcher);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_MATCHER_H_
