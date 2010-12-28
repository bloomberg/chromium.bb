// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ENCODING_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_ENCODING_MENU_CONTROLLER_H_
#pragma once

#include <utility>
#include <string>
#include <vector>

#include "base/basictypes.h"  // For DISALLOW_COPY_AND_ASSIGN
#include "base/gtest_prod_util.h"
#include "base/string16.h"

class Profile;

// Cross-platform logic needed for the encoding menu.
// For now, we don't need to track state so all methods are static.
class EncodingMenuController {
  FRIEND_TEST_ALL_PREFIXES(EncodingMenuControllerTest, EncodingIDsBelongTest);
  FRIEND_TEST_ALL_PREFIXES(EncodingMenuControllerTest, IsItemChecked);

 public:
  typedef std::pair<int, string16> EncodingMenuItem;
  typedef std::vector<EncodingMenuItem> EncodingMenuItemList;

 public:
  EncodingMenuController() {}

  // Given a command ID, does this command belong to the encoding menu?
  bool DoesCommandBelongToEncodingMenu(int id);

  // Returns true if the given encoding menu item (specified by item_id)
  // is checked.  Note that this header is included from objc, where the name
  // "id" is reserved.
  bool IsItemChecked(Profile* browser_profile,
                     const std::string& current_tab_encoding,
                     int item_id);

  // Fills in a list of menu items in the order they should appear in the menu.
  // Items whose ids are 0 are separators.
  void GetEncodingMenuItems(Profile* profile,
                            EncodingMenuItemList* menu_items);

 private:
  // List of all valid encoding GUI IDs.
  static const int kValidEncodingIds[];
  const int* ValidGUIEncodingIDs();
  int NumValidGUIEncodingIDs();

  DISALLOW_COPY_AND_ASSIGN(EncodingMenuController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ENCODING_MENU_CONTROLLER_H_
