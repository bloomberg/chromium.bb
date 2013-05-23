// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_LEARN_MORE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_LEARN_MORE_HANDLER_H_

#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
}

namespace options {

// Handler for the "Learn more" dialog available during creation of a managed
// user.
class ManagedUserLearnMoreHandler : public OptionsPageUIHandler {
 public:
  ManagedUserLearnMoreHandler();
  virtual ~ManagedUserLearnMoreHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedUserLearnMoreHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_LEARN_MORE_HANDLER_H_
