// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the Chrome To Mobile bubble, the transient
// bubble presented to send URLs and MHTML snapshots to a mobile device. There
// can only ever be a single bubble open, so the class presents only static
// methods, and handles the singleton behavior for you.  It also handles the
// object and widget lifetimes, destroying everything when the bubble is closed.

#ifndef CHROME_BROWSER_UI_GTK_CHROME_TO_MOBILE_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_CHROME_TO_MOBILE_BUBBLE_GTK_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/gtk/gtk_signal.h"

class Browser;

namespace base {
class DictionaryValue;
}

namespace ui {
class ThrobAnimation;
}

typedef struct _GtkWidget GtkWidget;

class ChromeToMobileBubbleGtk : public BubbleDelegateGtk,
                                public ui::AnimationDelegate,
                                public content::NotificationObserver,
                                public ChromeToMobileService::Observer {
 public:
  // Shows the Chrome to Mobile bubble, pointing at |anchor_widget|.
  // TODO(msw): Show the lit mobile icon during the bubble lifetime.
  static void Show(GtkWidget* anchor_widget, Browser* browser);

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ChromeToMobileService::Observer:
  virtual void SnapshotGenerated(const FilePath& path, int64 bytes) OVERRIDE;
  virtual void OnSendComplete(bool success) OVERRIDE;

 private:
  ChromeToMobileBubbleGtk(GtkWidget* anchor_widget, Browser* browser);
  virtual ~ChromeToMobileBubbleGtk();

  // Notified when |content_| is destroyed so we can delete our instance.
  CHROMEGTK_CALLBACK_0(ChromeToMobileBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(ChromeToMobileBubbleGtk, void, OnLearnClicked);
  CHROMEGTK_CALLBACK_0(ChromeToMobileBubbleGtk, void, OnCancelClicked);
  CHROMEGTK_CALLBACK_0(ChromeToMobileBubbleGtk, void, OnSendClicked);

  base::WeakPtrFactory<ChromeToMobileBubbleGtk> weak_ptr_factory_;

  // The browser that opened this bubble.
  Browser* browser_;

  // The Chrome To Mobile service associated with this bubble.
  ChromeToMobileService* service_;

  // Support members for getting theme colors and theme change notifications.
  GtkThemeService* theme_service_;
  content::NotificationRegistrar registrar_;

  // The file path for the MHTML page snapshot.
  FilePath snapshot_path_;

  // The list of radio buttons corresponding to the list of mobile devices.
  std::vector<GtkWidget*> radio_buttons_;

  // The widget to which this bubble is anchored.
  GtkWidget* anchor_widget_;

  // The labels in the bubble; tracked for theme changes.
  std::vector<GtkWidget*> labels_;

  GtkWidget* send_copy_;
  GtkWidget* learn_;
  GtkWidget* cancel_;
  GtkWidget* send_;
  GtkWidget* error_;

  // The actual BubbleGtk shown by this class and its content.
  BubbleGtk* bubble_;

  // An animation used to cycle through the "Sending..." status messages.
  scoped_ptr<ui::ThrobAnimation> progress_animation_;

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_CHROME_TO_MOBILE_BUBBLE_GTK_H_
