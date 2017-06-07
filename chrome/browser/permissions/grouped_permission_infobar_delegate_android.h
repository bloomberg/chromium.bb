// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>

#include "base/callback.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class GURL;
class InfoBarService;
class PermissionPromptAndroid;

// An InfoBar that displays a group of permission requests, each of which can be
// allowed or blocked independently.
// TODO(tsergeant): Expand this class so it can be used without subclassing.
class GroupedPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Public so we can have std::unique_ptr<GroupedPermissionInfoBarDelegate>.
  ~GroupedPermissionInfoBarDelegate() override;

  static infobars::InfoBar* Create(PermissionPromptAndroid* permission_prompt,
                                   InfoBarService* infobar_service,
                                   const GURL& requesting_origin);

  bool persist() const { return persist_; }
  void set_persist(bool persist) { persist_ = persist; }
  size_t PermissionCount() const;

  // Returns true if the infobar should display a toggle to allow users to
  // opt-out of persisting their accept/deny decision.
  bool ShouldShowPersistenceToggle() const;

  ContentSettingsType GetContentSettingType(size_t position) const;
  int GetIconIdForPermission(size_t position) const;

  // Message text to display for an individual permission at |position|.
  base::string16 GetMessageTextFragment(size_t position) const;

  // Toggle accept value for an individual permission at |position|.
  void ToggleAccept(size_t position, bool new_value);

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;
  base::string16 GetLinkText() const override;

 protected:
  bool GetAcceptState(size_t position);

 private:
  GroupedPermissionInfoBarDelegate(PermissionPromptAndroid* permission_prompt,
                                   const GURL& requesting_origin);

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  Type GetInfoBarType() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  GURL GetLinkURL() const override;

  // InfoBarDelegate:
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

  const GURL requesting_origin_;
  // Whether the accept/deny decision is persisted.
  bool persist_;
  PermissionPromptAndroid* permission_prompt_;

  DISALLOW_COPY_AND_ASSIGN(GroupedPermissionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
