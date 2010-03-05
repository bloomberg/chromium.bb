// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_APPCACHE_INFO_VIEW_H_
#define CHROME_BROWSER_VIEWS_APPCACHE_INFO_VIEW_H_

#include <string>
#include <vector>

#include "views/view.h"
#include "chrome/browser/browsing_data_appcache_helper.h"

namespace views {
class GridLayout;
class Label;
class Textfield;
}

///////////////////////////////////////////////////////////////////////////////
// AppCacheInfoView
//
//  Responsible for displaying a tabular grid of AppCache information.
class AppCacheInfoView : public views::View {
 public:
  AppCacheInfoView();
  virtual ~AppCacheInfoView();

  void SetAppCacheInfo(const appcache::AppCacheInfo* info);
  void ClearAppCacheDisplay();
  void EnableAppCacheDisplay(bool enabled);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(
      bool is_add, views::View* parent, views::View* child);

 private:
  void Init();
  void AddRow(int layout_id, views::GridLayout* layout, views::Label* label,
              views::Textfield* field, bool add_padding_row);

  views::Textfield* manifest_url_field_;
  views::Textfield* size_field_;
  views::Textfield* creation_date_field_;
  views::Textfield* last_access_field_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheInfoView);
};

#endif  // CHROME_BROWSER_VIEWS_APPCACHE_INFO_VIEW_H_

