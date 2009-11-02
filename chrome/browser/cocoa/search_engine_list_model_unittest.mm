// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/search_engine_list_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// A helper for NSNotifications. Makes a note that it's been called back.
@interface SearchEngineListHelper : NSObject {
 @public
  BOOL sawNotification_;
}
@end

@implementation SearchEngineListHelper
- (void)entryChanged:(NSNotification*)notify {
  sawNotification_ = YES;
}
@end

class SearchEngineListModelTest : public PlatformTest {
 public:
  SearchEngineListModelTest() {
    // Build a fake set of template urls.
    template_model_.reset(new TemplateURLModel(helper_.profile()));
    TemplateURL* t_url = new TemplateURL();
    t_url->SetURL(L"http://www.google.com/?q={searchTerms}", 0, 0);
    t_url->set_keyword(L"keyword");
    t_url->set_short_name(L"google");
    t_url->set_show_in_default_list(true);
    template_model_->Add(t_url);
    t_url = new TemplateURL();
    t_url->SetURL(L"http://www.google2.com/?q={searchTerms}", 0, 0);
    t_url->set_keyword(L"keyword2");
    t_url->set_short_name(L"google2");
    t_url->set_show_in_default_list(true);
    template_model_->Add(t_url);
    EXPECT_EQ(template_model_->GetTemplateURLs().size(), 2U);

    model_.reset([[SearchEngineListModel alloc]
                    initWithModel:template_model_.get()]);
    notification_helper_.reset([[SearchEngineListHelper alloc] init]);
    [[NSNotificationCenter defaultCenter]
        addObserver:notification_helper_.get()
           selector:@selector(entryChanged:)
              name:kSearchEngineListModelChangedNotification
            object:nil];
   }
  ~SearchEngineListModelTest() {
    [[NSNotificationCenter defaultCenter]
        removeObserver:notification_helper_.get()];
  }

  BrowserTestHelper helper_;
  scoped_ptr<TemplateURLModel> template_model_;
  scoped_nsobject<SearchEngineListModel> model_;
  scoped_nsobject<SearchEngineListHelper> notification_helper_;
};

TEST_F(SearchEngineListModelTest, Init) {
  scoped_nsobject<SearchEngineListModel> model(
      [[SearchEngineListModel alloc] initWithModel:template_model_.get()]);
}

TEST_F(SearchEngineListModelTest, Engines) {
  NSArray* engines = [model_ searchEngines];
  EXPECT_EQ([engines count], 2U);
}

TEST_F(SearchEngineListModelTest, Default) {
  EXPECT_EQ([model_ defaultIndex], 0U);

  [model_ setDefaultIndex:1];
  EXPECT_EQ([model_ defaultIndex], 1U);

  // Add two more URLs, neither of which are shown in the default list.
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL(L"http://www.google3.com/?q={searchTerms}", 0, 0);
  t_url->set_keyword(L"keyword3");
  t_url->set_short_name(L"google3 not eligible");
  t_url->set_show_in_default_list(false);
  template_model_->Add(t_url);
  t_url = new TemplateURL();
  t_url->SetURL(L"http://www.google4.com/?q={searchTerms}", 0, 0);
  t_url->set_keyword(L"keyword4");
  t_url->set_short_name(L"google4");
  t_url->set_show_in_default_list(false);
  template_model_->Add(t_url);

  // Still should only have 2 engines and not these newly added ones.
  EXPECT_EQ([[model_ searchEngines] count], 2U);

  // Since keyword3 is not in the default list, the 2nd index in the default
  // keyword list should be keyword4. Test for http://crbug.com/21898.
  template_model_->SetDefaultSearchProvider(t_url);
  EXPECT_EQ([[model_ searchEngines] count], 3U);
  EXPECT_EQ([model_ defaultIndex], 2U);

  NSString* defaultString = [[model_ searchEngines] objectAtIndex:2];
  EXPECT_TRUE([@"google4" isEqualToString:defaultString]);
}

// Make sure that when the back-end model changes that we get a notification.
TEST_F(SearchEngineListModelTest, Notification) {
  // Add one more item to force a notification.
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL(L"http://www.google3.com/foo/bar", 0, 0);
  t_url->set_keyword(L"keyword3");
  t_url->set_short_name(L"google3");
  t_url->set_show_in_default_list(true);
  template_model_->Add(t_url);

  EXPECT_TRUE(notification_helper_.get()->sawNotification_);
}
