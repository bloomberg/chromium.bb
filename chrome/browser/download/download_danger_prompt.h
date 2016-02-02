// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_

#include "base/callback_forward.h"

class GURL;

namespace content {
class DownloadItem;
class WebContents;
}

// Prompts the user for whether to Keep a dangerous DownloadItem using native
// UI. This prompt is invoked by the DownloadsDOMHandler when the user wants to
// accept a dangerous download. Having a native dialog intervene during the this
// workflow means that the chrome://downloads page no longer has the privilege
// to accept a dangerous download from script without user intervention. This
// step is necessary to prevent a malicious script form abusing such a
// privilege.
class DownloadDangerPrompt {
 public:
  // Actions resulting from showing the danger prompt.
  enum Action {
    ACCEPT,
    CANCEL,
    DISMISS,
  };
  typedef base::Callback<void(Action)> OnDone;

  // Return a new self-deleting DownloadDangerPrompt. |accepted| or |canceled|
  // will be run when the the respective action is invoked. |canceled| may also
  // be called when |item| is either no longer dangerous or no longer in
  // progress, or if the tab corresponding to |web_contents| is
  // closing. The returned DownloadDangerPrompt* is only used for testing. The
  // caller does not own the object and receive no guarantees about lifetime.
  // If |show_context|, then the prompt message will contain some information
  // about the download and its danger; otherwise it won't.
  static DownloadDangerPrompt* Create(content::DownloadItem* item,
                                      content::WebContents* web_contents,
                                      bool show_context,
                                      const OnDone& done);

  // Only to be used by tests. Subclasses must override to manually call the
  // respective button click handler.
  virtual void InvokeActionForTesting(Action action) = 0;

 protected:
  // Sends download recovery report to safe browsing backend.
  // Since it only records download url (DownloadItem::GetURL()), user's
  // action (click through or not) and its download danger type, it isn't gated
  // by user's extended reporting preference (i.e.
  // prefs::kSafeBrowsingExtendedReportingEnabled). We should not put any extra
  // information in this report.
  static void SendSafeBrowsingDownloadRecoveryReport(
      bool did_proceed,
      const content::DownloadItem& download);

  // Records UMA stats for a download danger prompt event.
  static void RecordDownloadDangerPrompt(bool did_proceed,
                                         const content::DownloadItem& download);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_
