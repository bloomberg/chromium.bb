// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_HANDLER_H_
#define CHROME_BROWSER_TABS_TAB_HANDLER_H_
#pragma once

class Browser;
class Profile;
class TabStripModel;

class TabHandlerDelegate {
 public:
  virtual Profile* GetProfile() const = 0;

  // TODO(beng): remove once decoupling with Browser is complete.
  virtual Browser* AsBrowser() = 0;
};

// An interface implemented by an object that can perform tab related
// functionality for a Browser. This functionality includes mapping individual
// TabContentses into indices for an index-based tab organization scheme for
// example.
class TabHandler {
 public:
  virtual ~TabHandler() {}

  // Creates a TabHandler implementation and returns it, transferring ownership
  // to the caller.
  static TabHandler* CreateTabHandler(TabHandlerDelegate* delegate);

  // TODO(beng): remove once decoupling with Browser is complete.
  virtual TabStripModel* GetTabStripModel() const = 0;
};

#endif  // CHROME_BROWSER_TABS_TAB_HANDLER_H_
