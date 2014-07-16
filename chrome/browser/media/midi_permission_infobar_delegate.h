// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_H_

#include <string>
#include "chrome/browser/content_settings/permission_infobar_delegate.h"
#include "chrome/common/content_settings_types.h"

class GURL;
class PermissionQueueController;
class InfoBarService;

// MidiPermissionInfoBarDelegates are created by the
// MidiPermissionContext to control the display and handling of MIDI permission
// infobars to the user.
class MidiPermissionInfoBarDelegate : public PermissionInfobarDelegate {
 public:
  // Creates a MIDI permission infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   PermissionQueueController* controller,
                                   const PermissionRequestID& id,
                                   const GURL& requesting_frame,
                                   const std::string& display_languages,
                                   ContentSettingsType type);

 private:
  MidiPermissionInfoBarDelegate(PermissionQueueController* controller,
                                const PermissionRequestID& id,
                                const GURL& requesting_frame,
                                int contents_unique_id,
                                const std::string& display_languages,
                                ContentSettingsType type);
  virtual ~MidiPermissionInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetIconID() const OVERRIDE;

  GURL requesting_frame_;
  std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(MidiPermissionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_H_
