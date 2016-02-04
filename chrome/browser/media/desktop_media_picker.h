// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_DESKTOP_MEDIA_PICKER_H_
#define CHROME_BROWSER_MEDIA_DESKTOP_MEDIA_PICKER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/gfx/native_widget_types.h"

class DesktopMediaList;

namespace content {
class WebContents;
}

// Abstract interface for desktop media picker UI. It's used by Desktop Media
// API to let user choose a desktop media source.
class DesktopMediaPicker {
 public:
  typedef base::Callback<void(content::DesktopMediaID)> DoneCallback;

  // Creates default implementation of DesktopMediaPicker for the current
  // platform.
  static scoped_ptr<DesktopMediaPicker> Create();

  DesktopMediaPicker() {}
  virtual ~DesktopMediaPicker() {}

  // Shows dialog with list of desktop media sources (screens, windows, tabs)
  // provided by |list| and calls |done_callback| when user chooses one of the
  // sources or closes the dialog.
  virtual void Show(content::WebContents* web_contents,
                    gfx::NativeWindow context,
                    gfx::NativeWindow parent,
                    const base::string16& app_name,
                    const base::string16& target_name,
                    scoped_ptr<DesktopMediaList> list,
                    bool request_audio,
                    const DoneCallback& done_callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPicker);
};

#endif  // CHROME_BROWSER_MEDIA_DESKTOP_MEDIA_PICKER_H_
