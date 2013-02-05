// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"

class GURL;
class TemplateURLService;
class TemplateURLServiceTestingProfile;
class TestingTemplateURLService;
class TestingProfile;
class WebDataService;

// Implements functionality to make it easier to test TemplateURLService and
// make changes to it.
class TemplateURLServiceTestUtil : public TemplateURLServiceObserver {
 public:
  TemplateURLServiceTestUtil();

  virtual ~TemplateURLServiceTestUtil();

  // Sets up the data structures for this class (mirroring gtest standard
  // methods).
  void SetUp();

  // Cleans up data structures for this class  (mirroring gtest standard
  // methods).
  void TearDown();

  // TemplateURLServiceObserver implemementation.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // Gets the observer count.
  int GetObserverCount();

  // Sets the observer count to 0.
  void ResetObserverCount();

  // Blocks the caller until the service has finished servicing all pending
  // requests.
  static void BlockTillServiceProcessesRequests();

  // Blocks the caller until the I/O thread has finished servicing all pending
  // requests.
  static void BlockTillIOThreadProcessesRequests();

  // Makes sure the load was successful and sent the correct notification.
  void VerifyLoad();

  // Makes the model believe it has been loaded (without actually doing the
  // load). Since this avoids setting the built-in keyword version, the next
  // load will do a merge from prepopulated data.
  void ChangeModelToLoadState();

  // Deletes the current model (and doesn't create a new one).
  void ClearModel();

  // Creates a new TemplateURLService.
  void ResetModel(bool verify_load);

  // Returns the search term from the last invocation of
  // TemplateURLService::SetKeywordSearchTermsForURL and clears the search term.
  string16 GetAndClearSearchTerm();

  // Set the google base url.  |base_url| must be valid.
  void SetGoogleBaseURL(const GURL& base_url) const;

  // Set the managed preferences for the default search provider and trigger
  // notification. If |alternate_url| is empty, uses an empty list of alternate
  // URLs, otherwise use a list containing a single entry.
  void SetManagedDefaultSearchPreferences(
    bool enabled,
    const std::string& name,
    const std::string& keyword,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& icon_url,
    const std::string& encodings,
    const std::string& alternate_url,
    const std::string& search_terms_replacement_key);

  // Remove all the managed preferences for the default search provider and
  // trigger notification.
  void RemoveManagedDefaultSearchPreferences();

  // Returns the TemplateURLService.
  TemplateURLService* model() const;

  // Returns the TestingProfile.
  TestingProfile* profile() const;

  // Starts an I/O thread.
  void StartIOThread();

  // Runs all pending tasks on the UI loop.
  void PumpLoop();

 private:
  MessageLoopForUI message_loop_;
  // Needed to make the DeleteOnUIThread trait of WebDataService work
  // properly.
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TemplateURLServiceTestingProfile> profile_;
  int changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceTestUtil);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
