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
}

template <typename T>
struct DefaultSingletonTraits;

// IOS implementation of ClipboardRecentContent
class ClipboardRecentContentIOS : public ClipboardRecentContent {
 public:
  static ClipboardRecentContentIOS* GetInstance();
  // Notifies that the content of the pasteboard may have changed.
  void PasteboardChanged();

  // ClipboardRecentContent implementation.
  bool GetRecentURLFromClipboard(GURL* url) const override;

 protected:
  // Protected for testing.
  ClipboardRecentContentIOS();
  ~ClipboardRecentContentIOS() override;

 private:
  friend struct DefaultSingletonTraits<ClipboardRecentContentIOS>;
  friend class test::ClipboardRecentContentIOSTestHelper;

  // Loads information from the user defaults about the latest pasteboard entry.
  void LoadFromUserDefaults();
  // Saves information to the user defaults about the latest pasteboard entry.
  void SaveToUserDefaults();
  // Returns the URL contained in the clipboard (if any).
  GURL URLFromPasteboard();
  // Returns whether the device has restarted since the last time a pasteboard
  // change was detected.
  bool DeviceRestartedSincePasteboardChanged();

  // The pasteboard's change count. Increases everytime the pasteboard changes.
  NSInteger lastPasteboardChangeCount_;
  // Estimation of the date when the pasteboard changed.
  base::scoped_nsobject<NSDate> lastPasteboardChangeDate_;
  // Cache of the GURL contained in the pasteboard (if any).
  GURL urlFromPasteboardCache_;
  // Bridge to receive notification when the pasteboard changes.
  base::scoped_nsobject<PasteboardNotificationListenerBridge>
      notificationBridge_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardRecentContentIOS);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
