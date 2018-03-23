// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_TABLE_VIEW_OWNING_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_TABLE_VIEW_OWNING_H_

// A protocol that can be implemented by anything that owns a table view.
@protocol TableViewOwning<NSObject>

@property(nonatomic, strong) UITableView* tableView;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_TABLE_VIEW_OWNING_H_
