// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/permissions/permission_util.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"

namespace infobars {
class InfoBar;
}

class InfoBarService;
class Profile;

// Base class for permission infobars, it implements the default behavior
// so that the accept/deny buttons grant/deny the relevant permission.
// A basic implementor only needs to implement the methods that
// provide an icon and a message text to the infobar.
class PermissionInfoBarDelegate : public ConfirmInfoBarDelegate {

 public:
  using PermissionSetCallback = base::Callback<void(bool, PermissionAction)>;

  // Creates an infobar for |type|. The returned pointer is owned by
  // |infobar_service| and manages its own lifetime; callers must only use it
  // for calls to |infobar_service|.
  static infobars::InfoBar* Create(content::PermissionType type,
                                   InfoBarService* infobar_service,
                                   const GURL& requesting_frame,
                                   bool user_gesture,
                                   Profile* profile,
                                   const PermissionSetCallback& callback);

  ~PermissionInfoBarDelegate() override;
  virtual std::vector<int> content_settings() const;

  // Returns true if the infobar should display a toggle to allow users to
  // opt-out of persisting their accept/deny decision.
  bool ShouldShowPersistenceToggle() const;

  // Sets whether or not a decided permission should be persisted to content
  // settings.
  void set_persist(bool persist) { persist_ = persist; }
  bool persist() const { return persist_; }

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;

 protected:
  PermissionInfoBarDelegate(const GURL& requesting_origin,
                            content::PermissionType permission_type,
                            ContentSettingsType content_settings_type,
                            bool user_gesture,
                            Profile* profile,
                            const PermissionSetCallback& callback);

 private:
  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  void InfoBarDismissed() override;
  PermissionInfoBarDelegate* AsPermissionInfoBarDelegate() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  virtual int GetMessageResourceId() const = 0;
  void SetPermission(bool update_content_setting, PermissionAction decision);

  GURL requesting_origin_;
  content::PermissionType permission_type_;
  ContentSettingsType content_settings_type_;
  Profile* const profile_;
  const PermissionSetCallback callback_;
  bool action_taken_;
  bool user_gesture_;
  bool persist_;

  DISALLOW_COPY_AND_ASSIGN(PermissionInfoBarDelegate);
};

// Implemented in platform-specific code.
std::unique_ptr<infobars::InfoBar> CreatePermissionInfoBar(
    std::unique_ptr<PermissionInfoBarDelegate> delegate);

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_
