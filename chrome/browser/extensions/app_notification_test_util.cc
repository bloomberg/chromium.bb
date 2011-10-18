// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/app_notification_test_util.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::IntToString;

namespace app_notification_test_util {

void ExpectListsEqual(const AppNotificationList& one,
                      const AppNotificationList& two) {
  ASSERT_EQ(one.size(), two.size());
  for (size_t i = 0; i < one.size(); i++) {
    ASSERT_TRUE(one[i]->Equals(*two[i]));
  }
}

void AddNotifications(AppNotificationList* list,
                      int count,
                      std::string prefix) {
  for (int i = 0; i < count; i++) {
    std::string title = prefix + "_title_" + IntToString(i);
    std::string body = prefix + "_body_" + IntToString(i);
    AppNotification* item = new AppNotification(title, body);
    if (i % 2 == 0) {
      item->set_link_url(GURL("http://www.example.com/" + prefix));
      item->set_link_text(prefix + "_link_" + IntToString(i));
    }
    list->push_back(linked_ptr<AppNotification>(item));
  }
}

AppNotification* CopyAppNotification(const AppNotification& source) {
  AppNotification* copy = new AppNotification(source.title(), source.body());
  copy->set_link_url(source.link_url());
  copy->set_link_text(source.link_text());
  return copy;
}

void AddCopiesFromList(AppNotificationManager* manager,
                       const std::string& extension_id,
                       const AppNotificationList& list) {
  for (AppNotificationList::const_iterator i = list.begin();
       i != list.end();
       ++i) {
    manager->Add(extension_id, CopyAppNotification(*(i->get())));
  }
}

}  // namespace app_notification_test_util
