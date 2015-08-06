// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_

#include "base/mac/scoped_nsobject.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "url/gurl.h"

@class NSDate;
@class PasteboardNotificationListenerBridge;

class ClipboardRecentContentIOSTest;

// IOS implementation of ClipboardRecentContent
class ClipboardRecentContentIOS : public ClipboardRecentContent {
 public:
  // |application_scheme| is the URL scheme that can be used to open the
  // current application, may be empty if no such scheme exists. Used to
  // determine whether or not the clipboard contains a relevant URL.
  explicit ClipboardRecentContentIOS(const std::string& application_scheme);
  ~ClipboardRecentContentIOS() override;

  // Notifies that the content of the pasteboard may have changed.
  void PasteboardChanged();

  // ClipboardRecentContent implementation.
  bool GetRecentURLFromClipboard(GURL* url) const override;
  base::TimeDelta GetClipboardContentAge() const override;
  void SuppressClipboardContent() override;

 private:
  friend class ClipboardRecentContentIOSTest;

  // Helper constructor for testing. |uptime| is how long ago the device has
  // started, while |application_scheme| has the same meaning as the public
  // constructor.
  ClipboardRecentContentIOS(const std::string& application_scheme,
                            base::TimeDelta uptime);

  // Initializes the object. |uptime| is how long ago the device has started.
  void Init(base::TimeDelta uptime);

  // Loads information from the user defaults about the latest pasteboard entry.
  void LoadFromUserDefaults();

  // Saves information to the user defaults about the latest pasteboard entry.
  void SaveToUserDefaults();

  // Returns the URL contained in the clipboard (if any).
  GURL URLFromPasteboard();

  // Contains the URL scheme opening the app. May be empty.
  std::string application_scheme_;
  // The pasteboard's change count. Increases everytime the pasteboard changes.
  NSInteger lastPasteboardChangeCount_;
  // Estimation of the date when the pasteboard changed.
  base::scoped_nsobject<NSDate> lastPasteboardChangeDate_;
  // Cache of the GURL contained in the pasteboard (if any).
  GURL urlFromPasteboardCache_;
  // A count identifying the suppressed pasteboard entry. Contains
  // |NSIntegerMax| if there's no relevant entry to suppress.
  NSInteger suppressedPasteboardEntryCount_;
  // Bridge to receive notification when the pasteboard changes.
  base::scoped_nsobject<PasteboardNotificationListenerBridge>
      notificationBridge_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardRecentContentIOS);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
