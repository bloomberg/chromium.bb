// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MIDI_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_MIDI_PERMISSION_CONTEXT_H_

#include "chrome/browser/content_settings/permission_context_base.h"

class GURL;
class PermissionRequestID;

class MidiPermissionContext : public PermissionContextBase {
 public:
  explicit MidiPermissionContext(Profile* profile);
  virtual ~MidiPermissionContext();

 private:

  // PermissionContextBase:
  virtual void UpdateTabContext(const PermissionRequestID& id,
                                const GURL& requesting_frame,
                                bool allowed) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MidiPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_MIDI_PERMISSION_CONTEXT_H_
