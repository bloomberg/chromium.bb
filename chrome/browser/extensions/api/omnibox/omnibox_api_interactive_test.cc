// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/omnibox_api_testbase.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"


// Tests that the autocomplete popup doesn't reopen after accepting input for
// a given query.
// http://crbug.com/88552
IN_PROC_BROWSER_TEST_F(OmniboxApiTest, PopupStaysClosed) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  // The results depend on the TemplateURLService being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  ui_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  LocationBar* location_bar = GetLocationBar(browser());
  OmniboxView* omnibox_view = location_bar->GetLocationEntry();
  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(browser());
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();

  // Input a keyword query and wait for suggestions from the extension.
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(ASCIIToUTF16("keyword comman"));
  omnibox_view->OnAfterPossibleChange();
  WaitForAutocompleteDone(autocomplete_controller);
  EXPECT_TRUE(autocomplete_controller->done());
  EXPECT_TRUE(popup_model->IsOpen());

  // Quickly type another query and accept it before getting suggestions back
  // for the query. The popup will close after accepting input - ensure that it
  // does not reopen when the extension returns its suggestions.
  ResultCatcher catcher;

  // TODO: Rather than send this second request by talking to the controller
  // directly, figure out how to send it via the proper calls to
  // location_bar or location_bar->().
  autocomplete_controller->Start(
      AutocompleteInput(ASCIIToUTF16("keyword command"), string16::npos,
                        string16(), GURL(), true, false, true,
                        AutocompleteInput::ALL_MATCHES));
  location_bar->AcceptInput();
  WaitForAutocompleteDone(autocomplete_controller);
  EXPECT_TRUE(autocomplete_controller->done());
  // This checks that the keyword provider (via javascript)
  // gets told to navigate to the string "command".
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(popup_model->IsOpen());
}
