// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_SCRIPT_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_SCRIPT_BUBBLE_GTK_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/image/image.h"

class ContentSettingBubbleModel;
class Profile;

namespace content {
class WebContents;
}

// A bubble that is displayed to show users when extensions are interacting
// with the current page.
class ScriptBubbleGtk : public BubbleDelegateGtk {
 public:
  static void Show(GtkWidget* anchor, content::WebContents* web_contents);

 private:
  void Close();

  virtual ~ScriptBubbleGtk();

  ScriptBubbleGtk(GtkWidget* anchor, content::WebContents* web_contents);

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // Builds the bubble and all the widgets that it displays.
  void BuildBubble();

  void OnIconLoaded(const std::string& extension_id, const gfx::Image& icon);

  // Widget callback methods.
  static void OnItemLinkClickedThunk(GtkWidget* sender, void* user_data);
  void OnItemLinkClicked(GtkWidget* sender);

  // We position the bubble near this widget.
  GtkWidget* anchor_;

  // The WebContents we're being displayed for.
  content::WebContents* web_contents_;

  // The active profile.
  Profile* profile_;

  // The bubble.
  BubbleGtk* bubble_;

  std::map<std::string, GtkImage*> icon_controls_;
  std::map<GtkWidget*, std::string> link_controls_;

  base::WeakPtrFactory<ScriptBubbleGtk> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_GTK_SCRIPT_BUBBLE_GTK_H_
