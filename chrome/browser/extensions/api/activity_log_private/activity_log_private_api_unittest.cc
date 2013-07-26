// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kExtensionId[] = "extensionid";
const char kApiCall[] = "api.call";
const char kArgs[] = "1, 2";
const char kExtra[] = "extra";

}  // extensions

namespace extensions {

using api::activity_log_private::BlockedChromeActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ExtensionActivity;

class ActivityLogApiUnitTest : public testing::Test {
};

// TODO(felt): These are the old unit tests from before the activity log Action
// class and database refactoring.  These need to be updated, but since the
// private API will be reworked anyway these are just disabled for now.
#if 0
TEST_F(ActivityLogApiUnitTest, ConvertBlockedAction) {
  scoped_refptr<Action> action(
      new BlockedAction(kExtensionId,
                        base::Time::Now(),
                        kApiCall,
                        kArgs,
                        BlockedAction::ACCESS_DENIED,
                        kExtra));
  scoped_ptr<ExtensionActivity> result =
      action->ConvertToExtensionActivity();
  ASSERT_EQ(ExtensionActivity::ACTIVITY_TYPE_BLOCKED_CHROME,
            result->activity_type);
  ASSERT_EQ(kExtensionId, *(result->extension_id.get()));
  ASSERT_EQ(kApiCall,
            *(result->blocked_chrome_activity_detail->api_call.get()));
  ASSERT_EQ(kArgs,
            *(result->blocked_chrome_activity_detail->args.get()));
  ASSERT_EQ(BlockedChromeActivityDetail::REASON_ACCESS_DENIED,
            result->blocked_chrome_activity_detail->reason);
  ASSERT_EQ(kExtra,
            *(result->blocked_chrome_activity_detail->extra.get()));
}

TEST_F(ActivityLogApiUnitTest, ConvertChromeApiAction) {
  scoped_refptr<Action> action(
    new APIAction(kExtensionId,
                  base::Time::Now(),
                  APIAction::CALL,
                  kApiCall,
                  kArgs,
                  base::ListValue(),
                  kExtra));
  scoped_ptr<ExtensionActivity> result =
      action->ConvertToExtensionActivity();
  ASSERT_EQ(ExtensionActivity::ACTIVITY_TYPE_CHROME,
            result->activity_type);
  ASSERT_EQ(kExtensionId, *(result->extension_id.get()));
  ASSERT_EQ(ChromeActivityDetail::API_ACTIVITY_TYPE_CALL,
            result->chrome_activity_detail->api_activity_type);
  ASSERT_EQ(kApiCall,
            *(result->chrome_activity_detail->api_call.get()));
  ASSERT_EQ(kArgs,
            *(result->chrome_activity_detail->args.get()));
  ASSERT_EQ(kExtra,
            *(result->chrome_activity_detail->extra.get()));
}

TEST_F(ActivityLogApiUnitTest, ConvertDomAction) {
  scoped_refptr<Action> action(
      new DOMAction(kExtensionId,
                    base::Time::Now(),
                    DomActionType::SETTER,
                    GURL("http://www.google.com"),
                    base::ASCIIToUTF16("Title"),
                    kApiCall,
                    kArgs,
                    kExtra));
  scoped_ptr<ExtensionActivity> result =
      action->ConvertToExtensionActivity();
  ASSERT_EQ(ExtensionActivity::ACTIVITY_TYPE_DOM, result->activity_type);
  ASSERT_EQ(kExtensionId, *(result->extension_id.get()));
  ASSERT_EQ(DomActivityDetail::DOM_ACTIVITY_TYPE_SETTER,
            result->dom_activity_detail->dom_activity_type);
  ASSERT_EQ("http://www.google.com/",
            *(result->dom_activity_detail->url.get()));
  ASSERT_EQ("Title", *(result->dom_activity_detail->url_title.get()));
  ASSERT_EQ(kApiCall,
            *(result->dom_activity_detail->api_call.get()));
  ASSERT_EQ(kArgs,
            *(result->dom_activity_detail->args.get()));
  ASSERT_EQ(kExtra,
            *(result->dom_activity_detail->extra.get()));
}
#endif

}  // namespace extensions
