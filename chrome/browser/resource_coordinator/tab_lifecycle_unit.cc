// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"

namespace resource_coordinator {

namespace {

class TabLifecycleUnitHolder
    : public content::WebContentsUserData<TabLifecycleUnitHolder> {
 public:
  explicit TabLifecycleUnitHolder(content::WebContents*) {}
  TabLifecycleUnit* tab_lifecycle_unit() const { return tab_lifecycle_unit_; }
  void set_tab_lifecycle_unit(TabLifecycleUnit* tab_lifecycle_unit) {
    tab_lifecycle_unit_ = tab_lifecycle_unit;
  }

 private:
  TabLifecycleUnit* tab_lifecycle_unit_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitHolder);
};

}  // namespace

// static
TabLifecycleUnit* TabLifecycleUnit::FromWebContents(
    content::WebContents* web_contents) {
  TabLifecycleUnitHolder* holder =
      TabLifecycleUnitHolder::FromWebContents(web_contents);
  return holder ? holder->tab_lifecycle_unit() : nullptr;
}

// static
void TabLifecycleUnit::SetForWebContents(content::WebContents* web_contents,
                                         TabLifecycleUnit* tab_lifecycle_unit) {
  TabLifecycleUnitHolder::CreateForWebContents(web_contents);
  TabLifecycleUnitHolder::FromWebContents(web_contents)
      ->set_tab_lifecycle_unit(tab_lifecycle_unit);
}

}  // namespace resource_coordinator

DEFINE_WEB_CONTENTS_USER_DATA_KEY(resource_coordinator::TabLifecycleUnitHolder);
