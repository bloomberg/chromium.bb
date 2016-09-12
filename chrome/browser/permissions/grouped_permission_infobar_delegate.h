// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class GURL;
class InfoBarService;

namespace infobars {
class InfoBarManager;
}

// Configures an InfoBar to display a group of permission requests, each of
// which can be allowed or blocked independently.
// TODO(tsergeant): Expand this class so it can be used without subclassing.
class GroupedPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Implementation is in platform-specific infobar file.
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      infobars::InfoBarManager* infobar_manager,
      std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate);

  GroupedPermissionInfoBarDelegate(
      const GURL& requesting_origin,
      const std::vector<ContentSettingsType>& types);
  ~GroupedPermissionInfoBarDelegate() override;

  base::string16 GetMessageText() const override;

  int GetPermissionCount() const;
  ContentSettingsType GetContentSettingType(int index) const;
  int GetIconIdForPermission(int index) const;
  // Message text to display for an individual permission at |index|.
  base::string16 GetMessageTextFragment(int index) const;

  void ToggleAccept(int position, bool new_value);

 protected:
  bool GetAcceptState(int position);

 private:
  // ConfirmInfoBarDelegate:
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  int GetButtons() const override;

  // InfoBarDelegate:
  Type GetInfoBarType() const override;

  const GURL requesting_origin_;
  const std::vector<ContentSettingsType> types_;
  std::vector<bool> accept_states_;

  DISALLOW_COPY_AND_ASSIGN(GroupedPermissionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_H_
