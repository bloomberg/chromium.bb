// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIES_TABLE_MODEL_H_
#define CHROME_BROWSER_COOKIES_TABLE_MODEL_H_

#include <string>
#include <vector>

#include "app/table_model.h"
#include "net/base/cookie_monster.h"

class Profile;

class CookiesTableModel : public TableModel {
 public:
  explicit CookiesTableModel(Profile* profile);
  virtual ~CookiesTableModel() {}

  // Returns information about the Cookie at the specified index.
  std::string GetDomainAt(int index);
  net::CookieMonster::CanonicalCookie& GetCookieAt(int index);

  // Remove the specified cookies from the Cookie Monster and update the view.
  void RemoveCookies(int start_index, int remove_count);
  void RemoveAllShownCookies();

  // TableModel methods.
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual SkBitmap GetIcon(int row);
  virtual int CompareValues(int row1, int row2, int column_id);
  virtual void SetObserver(TableModelObserver* observer);

  // Filter the cookies to only display matched results.
  void UpdateSearchResults(const std::wstring& filter);

 private:
  void LoadCookies();
  void DoFilter();

  std::wstring filter_;

  // The profile from which this model sources cookies.
  Profile* profile_;

  typedef net::CookieMonster::CookieList CookieList;
  typedef std::vector<net::CookieMonster::CookieListPair*> CookiePtrList;
  CookieList all_cookies_;
  CookiePtrList shown_cookies_;

  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTableModel);
};

#endif  // CHROME_BROWSER_COOKIES_TABLE_MODEL_H_
