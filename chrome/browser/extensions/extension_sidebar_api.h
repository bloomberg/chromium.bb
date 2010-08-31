// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDEBAR_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDEBAR_API_H_

#include <string>
#include "chrome/browser/extensions/extension_function.h"

class DictionaryValue;
class Profile;
class RenderViewHost;
class TabContents;

namespace extension_sidebar_constants {
extern const char kActiveState[];
extern const char kHiddenState[];
extern const char kShownState[];
}  // namespace extension_sidebar_constants

// Event router class for events related to the sidebar API.
class ExtensionSidebarEventRouter {
 public:
  // Sidebar state changed.
  static void OnStateChanged(
      Profile* profile, TabContents* tab, const std::string& content_id,
      const std::string& state);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionSidebarEventRouter);
};

// Base class for sidebar function APIs.
class SidebarFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details) = 0;
};

class CollapseSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.collapse");
};

class ExpandSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.expand");
};

class GetStateSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.getState");
};

class HideSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.hide");
};

class NavigateSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.navigate");
};

class SetBadgeTextSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.setBadgeText");
};

class SetIconSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.setIcon");
};

class SetTitleSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.setTitle");
};

class ShowSidebarFunction : public SidebarFunction {
 private:
  virtual bool RunImpl(TabContents* tab,
                       const std::string& content_id,
                       const DictionaryValue& details);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.sidebar.show");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDEBAR_API_H_

