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

namespace test {
class ClipboardRecentContentIOSTestHelper;
}  // namespace test

template <typename T>
struct DefaultSingletonTraits;

// IOS implementation of ClipboardRecentContent
class ClipboardRecentContentIOS : public ClipboardRecentContent {
 public:
  static ClipboardRecentContentIOS* GetInstance();
  // Notifies that the content of the pasteboard may have changed.
  void PasteboardChanged();

  bool GetRecentURLFromClipboard(GURL* url) const override;

  base::TimeDelta GetClipboardContentAge() const override;

  void SuppressClipboardContent() override;

 protected:
  // Protected for testing.
  ClipboardRecentContentIOS();
  ~ClipboardRecentContentIOS() override;
  // |uptime| is how long ago the device has started.
  ClipboardRecentContentIOS(base::TimeDelta uptime);

 private:
  friend struct DefaultSingletonTraits<ClipboardRecentContentIOS>;
  friend class test::ClipboardRecentContentIOSTestHelper;
  // Initializes the object. |uptime| is how long ago the device has started.
  void Init(base::TimeDelta uptime);
  // Loads information from the user defaults about the latest pasteboard entry.
  void LoadFromUserDefaults();
  // Saves information to the user defaults about the latest pasteboard entry.
  void SaveToUserDefaults();
  // Returns the URL contained in the clipboard (if any).
  GURL URLFromPasteboard();

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
