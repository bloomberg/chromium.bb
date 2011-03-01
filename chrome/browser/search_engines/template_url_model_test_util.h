// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_TEST_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "content/browser/browser_thread.h"

class TemplateURLModel;
class TemplateURLModelTestingProfile;
class TestingTemplateURLModel;
class TestingProfile;
class WebDataService;

// Implements functionality to make it easier to test TemplateURLModel and
// make changes to it.
class TemplateURLModelTestUtil : public TemplateURLModelObserver {
 public:
  TemplateURLModelTestUtil();

  virtual ~TemplateURLModelTestUtil();

  // Sets up the data structures for this class (mirroring gtest standard
  // methods).
  void SetUp();

  // Cleans up data structures for this class  (mirroring gtest standard
  // methods).
  void TearDown();

  // TemplateURLModelObserver implemementation.
  virtual void OnTemplateURLModelChanged();

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

  // Creates a new TemplateURLModel.
  void ResetModel(bool verify_load);

  // Returns the search term from the last invocation of
  // TemplateURLModel::SetKeywordSearchTermsForURL and clears the search term.
  string16 GetAndClearSearchTerm();

  // Set the google base url.
  void SetGoogleBaseURL(const std::string& base_url) const;

  // Returns the WebDataService.
  WebDataService* GetWebDataService();

  // Returns the TemplateURLModel.
  TemplateURLModel* model() const;

  // Returns the TestingProfile.
  TestingProfile* profile() const;

  // Starts an I/O thread.
  void StartIOThread();

 private:
  MessageLoopForUI message_loop_;
  // Needed to make the DeleteOnUIThread trait of WebDataService work
  // properly.
  BrowserThread ui_thread_;
  scoped_ptr<TemplateURLModelTestingProfile> profile_;
  scoped_ptr<TestingTemplateURLModel> model_;
  int changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLModelTestUtil);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_TEST_UTIL_H_
