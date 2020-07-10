// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_TEST_UTIL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"

class MockMediaGalleriesDialog : public MediaGalleriesDialog {
 public:
  typedef base::Callback<void(int update_count)> DialogDestroyedCallback;

  explicit MockMediaGalleriesDialog(const DialogDestroyedCallback& callback);
  ~MockMediaGalleriesDialog() override;

  // MediaGalleriesDialog implementation.
  void UpdateGalleries() override;

  // Number up times UpdateResults has been called.
  int update_count() const;

 private:
  // MediaGalleriesDialog implementation.
  void AcceptDialogForTesting() override;

  int update_count_;

  DialogDestroyedCallback dialog_destroyed_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaGalleriesDialog);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_TEST_UTIL_H_
