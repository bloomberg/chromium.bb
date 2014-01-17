// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_util.h"

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"

namespace task_manager {

namespace util {

int GetMessagePrefixID(bool is_app,
                       bool is_extension,
                       bool is_incognito,
                       bool is_prerender,
                       bool is_background) {
  if (is_app) {
    if (is_background)
      return IDS_TASK_MANAGER_BACKGROUND_PREFIX;
    if (is_incognito)
      return IDS_TASK_MANAGER_APP_INCOGNITO_PREFIX;
    return IDS_TASK_MANAGER_APP_PREFIX;
  }
  if (is_extension) {
    if (is_incognito)
      return IDS_TASK_MANAGER_EXTENSION_INCOGNITO_PREFIX;
    return IDS_TASK_MANAGER_EXTENSION_PREFIX;
  }
  if (is_prerender)
    return IDS_TASK_MANAGER_PRERENDER_PREFIX;
  if (is_incognito)
    return IDS_TASK_MANAGER_TAB_INCOGNITO_PREFIX;

  return IDS_TASK_MANAGER_TAB_PREFIX;
}

base::string16 GetProfileNameFromInfoCache(Profile* profile) {
  DCHECK(profile);

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(
      profile->GetOriginalProfile()->GetPath());
  if (index == std::string::npos)
    return base::string16();
  else
    return cache.GetNameOfProfileAtIndex(index);
}

base::string16 GetTitleFromWebContents(content::WebContents* web_contents) {
  DCHECK(web_contents);

  base::string16 title = web_contents->GetTitle();
  if (title.empty()) {
    GURL url = web_contents->GetURL();
    title = base::UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    title = base::i18n::GetDisplayStringInLTRDirectionality(title);
  } else {
    // Since the tab_title will be concatenated with
    // IDS_TASK_MANAGER_TAB_PREFIX, we need to explicitly set the tab_title to
    // be LTR format if there is no strong RTL charater in it. Otherwise, if
    // IDS_TASK_MANAGER_TAB_PREFIX is an RTL word, the concatenated result
    // might be wrong. For example, http://mail.yahoo.com, whose title is
    // "Yahoo! Mail: The best web-based Email!", without setting it explicitly
    // as LTR format, the concatenated result will be "!Yahoo! Mail: The best
    // web-based Email :BAT", in which the capital letters "BAT" stands for
    // the Hebrew word for "tab".
    base::i18n::AdjustStringForLocaleDirection(&title);
  }
  return title;
}

}  // namespace util

}  // namespace task_manager
