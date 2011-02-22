// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/value_helper.h"

#include "chrome/browser/webui/new_tab_ui.h"
#include "chrome/common/url_constants.h"

bool ValueHelper::TabToValue(
    const TabRestoreService::Tab& tab,
    DictionaryValue* dictionary) {
  if (tab.navigations.empty())
    return false;

  const TabNavigation& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  if (current_navigation.virtual_url() == GURL(chrome::kChromeUINewTabURL))
    return false;
  NewTabUI::SetURLTitleAndDirection(dictionary, current_navigation.title(),
                                    current_navigation.virtual_url());
  dictionary->SetString("type", "tab");
  dictionary->SetDouble("timestamp", tab.timestamp.ToDoubleT());
  return true;
}

bool ValueHelper::WindowToValue(
    const TabRestoreService::Window& window,
    DictionaryValue* dictionary) {
  if (window.tabs.empty()) {
    NOTREACHED();
    return false;
  }
  scoped_ptr<ListValue> tab_values(new ListValue());
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    scoped_ptr<DictionaryValue> tab_value(new DictionaryValue());
    if (TabToValue(window.tabs[i], tab_value.get()))
      tab_values->Append(tab_value.release());
  }
  if (tab_values->GetSize() == 0)
    return false;
  dictionary->SetString("type", "window");
  dictionary->SetDouble("timestamp", window.timestamp.ToDoubleT());
  dictionary->Set("tabs", tab_values.release());
  return true;
}

