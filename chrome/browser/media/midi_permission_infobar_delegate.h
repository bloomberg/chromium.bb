// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "url/gurl.h"

class PermissionQueueController;
class InfoBarService;

// MidiPermissionInfoBarDelegates are created by the
// ChromeMidiPermissionContext to control the display and handling of MIDI
// permission infobars to the user.
class MidiPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a MIDI permission infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static InfoBar* Create(InfoBarService* infobar_service,
                         PermissionQueueController* controller,
                         const PermissionRequestID& id,
                         const GURL& requesting_frame,
                         const std::string& display_languages);

 private:
  MidiPermissionInfoBarDelegate(PermissionQueueController* controller,
                                const PermissionRequestID& id,
                                const GURL& requesting_frame,
                                int contents_unique_id,
                                const std::string& display_languages);
  virtual ~MidiPermissionInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // Calls back to the controller to inform it of the user's decision.
  void SetPermission(bool update_content_setting, bool allowed);

 private:
  PermissionQueueController* controller_;
  const PermissionRequestID id_;
  GURL requesting_frame_;
  int contents_unique_id_;
  std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(MidiPermissionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_H_
