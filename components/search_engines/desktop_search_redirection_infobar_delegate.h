// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_INFOBAR_DELEGATE_H_
#define COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class PrefService;

namespace infobars {
class InfoBarManager;
}  // namespace infobars

// Informs the user that a desktop search has been redirected to the default
// search engine.
class DesktopSearchRedirectionInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a search redirection infobar and delegate and adds the infobar to
  // |infobar_manager|.  Records in |pref_service| that such an infobar was
  // shown. |default_search_engine_name| is the name of the default search
  // engine. |manage_search_settings_callback| should open the search settings
  // page.
  static void Show(infobars::InfoBarManager* infobar_manager,
                   const base::string16& default_search_engine_name,
                   const base::Closure& manage_search_settings_callback,
                   PrefService* pref_service);

 private:
  DesktopSearchRedirectionInfobarDelegate(
      const base::string16& default_search_engine_name,
      const base::Closure& manage_search_settings_callback);
  ~DesktopSearchRedirectionInfobarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  base::string16 default_search_engine_name_;
  base::Closure manage_search_settings_callback_;

  // True when the infobar has been closed explicitly by the user.
  bool closed_by_user_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSearchRedirectionInfobarDelegate);
};

#endif  // COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_INFOBAR_DELEGATE_H_
