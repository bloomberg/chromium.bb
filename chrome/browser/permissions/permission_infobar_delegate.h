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
#include "url/gurl.h"

namespace infobars {
class InfoBar;
}

class InfoBarService;
class Profile;

// Base delegate class for permission prompts on Android. The default behavior
// is that the accept/deny buttons grant/deny the relevant permission
// respectively. A basic implementor only needs to implement the methods that
// provide an icon and a message text to the infobar.
//
// By default, the user is presented with an infobar to make their choice. If
// the experimental ModalPermissionPrompts feature is active, they will instead
// see a modal dialog. Currently, this class is used for both; future
// refactoring will be undertaken based on whether support for infobars is
// retained following the modal prompt experiment.
class PermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using PermissionSetCallback = base::Callback<void(bool, PermissionAction)>;

  // Creates an infobar for |type|. The returned pointer is owned by
  // |infobar_service| and manages its own lifetime; callers must only use it
  // for calls to |infobar_service|.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   ContentSettingsType type,
                                   const GURL& requesting_frame,
                                   bool user_gesture,
                                   Profile* profile,
                                   const PermissionSetCallback& callback);

  static std::unique_ptr<PermissionInfoBarDelegate> CreateDelegate(
      ContentSettingsType type,
      const GURL& requesting_frame,
      bool user_gesture,
      Profile* profile,
      const PermissionSetCallback& callback);

  ~PermissionInfoBarDelegate() override;
  virtual std::vector<int> content_settings_types() const;

  // Returns true if the infobar should display a toggle to allow users to
  // opt-out of persisting their accept/deny decision.
  bool ShouldShowPersistenceToggle() const;

  // Sets whether or not a decided permission should be persisted to content
  // settings.
  void set_persist(bool persist) { persist_ = persist; }
  bool persist() const { return persist_; }

  bool user_gesture() const { return user_gesture_; }

  // ConfirmInfoBarDelegate:
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  base::string16 GetMessageText() const override;

 protected:
  PermissionInfoBarDelegate(const GURL& requesting_origin,
                            ContentSettingsType content_settings_type,
                            bool user_gesture,
                            Profile* profile,
                            const PermissionSetCallback& callback);

 private:
  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  PermissionInfoBarDelegate* AsPermissionInfoBarDelegate() override;

  virtual int GetMessageResourceId() const = 0;
  void SetPermission(bool update_content_setting, PermissionAction decision);

  GURL requesting_origin_;
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
