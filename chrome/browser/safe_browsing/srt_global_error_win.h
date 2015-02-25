// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_GLOBAL_ERROR_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_GLOBAL_ERROR_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/global_error/global_error.h"

class GlobalErrorService;

// Encapsulates UI-related functionality for the software removal tool (SRT)
// prompt. The UI consists of two parts: (1.) the profile reset (pop-up) bubble,
// and (2.) a menu item in the wrench menu (provided by being a GlobalError).
class SRTGlobalError : public GlobalErrorWithStandardBubble {
 public:
  // Creates the new SRTGlobalError to be associated to the given
  // |global_error_service|. |download_path| specifies the path the SRT has
  // already been downloaded to. If for some reason the SRT cannot be found at
  // this path, if the path is empty or if the SRT cannot be launched, the
  // bubble will fall back to opening the SRT download page if the user requests
  // cleanup.
  SRTGlobalError(GlobalErrorService* global_error_service,
                 const base::FilePath& downloaded_path);
  ~SRTGlobalError() override;

  // GlobalError:
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  void ShowBubbleView(Browser* browser) override;

  // GlobalErrorWithStandardBubble:
  base::string16 GetBubbleViewTitle() override;
  std::vector<base::string16> GetBubbleViewMessages() override;
  base::string16 GetBubbleViewAcceptButtonLabel() override;
  base::string16 GetBubbleViewCancelButtonLabel() override;
  void OnBubbleViewDidClose(Browser* browser) override;
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  bool ShouldAddElevationIconToAcceptButton() override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;
  bool ShouldCloseOnDeactivate() const override;

 private:
  // Executes the SRT if the executable is present.
  void MaybeExecuteSRT();

  // Falls back to a navigation to the download page when we failed to
  // download and execute the SRT.
  void FallbackToDownloadPage();

  // Destroys this instance.
  void DestroySelf();

  // Used to dismiss the GlobalError, then set to NULL.
  GlobalErrorService* global_error_service_;

  // The path to the downloaded executable.
  base::FilePath downloaded_path_;

  DISALLOW_COPY_AND_ASSIGN(SRTGlobalError);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_GLOBAL_ERROR_WIN_H_
