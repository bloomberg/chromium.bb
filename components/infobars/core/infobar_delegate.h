// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/base/window_open_disposition.h"

class AutoLoginInfoBarDelegate;
class ConfirmInfoBarDelegate;
class ExtensionInfoBarDelegate;
class InsecureContentInfoBarDelegate;
class MediaStreamInfoBarDelegate;
class PopupBlockedInfoBarDelegate;
class RegisterProtocolHandlerInfoBarDelegate;
class ScreenCaptureInfoBarDelegate;
class ThemeInstalledInfoBarDelegate;
class ThreeDAPIInfoBarDelegate;

namespace translate {
class TranslateInfoBarDelegate;
}

namespace gfx {
class Image;
}

namespace infobars {

class InfoBar;

// An interface implemented by objects wishing to control an InfoBar.
// Implementing this interface is not sufficient to use an InfoBar, since it
// does not map to a specific InfoBar type. Instead, you must implement
// ConfirmInfoBarDelegate, or override with your own delegate for your own
// InfoBar variety.
class InfoBarDelegate {
 public:
  // The type of the infobar. It controls its appearance, such as its background
  // color.
  enum Type {
    WARNING_TYPE,
    PAGE_ACTION_TYPE,
  };

  enum InfoBarAutomationType {
    CONFIRM_INFOBAR,
    PASSWORD_INFOBAR,
    RPH_INFOBAR,
    UNKNOWN_INFOBAR,
  };

  // Describes navigation events, used to decide whether infobars should be
  // dismissed.
  struct NavigationDetails {
    // Unique identifier for the entry.
    int entry_id;
    // True if it is a navigation to a different page (as opposed to in-page).
    bool is_navigation_to_different_page;
    // True if the entry replaced the existing one.
    bool did_replace_entry;
    // True for the main frame, false for a sub-frame.
    bool is_main_frame;
    bool is_reload;
    bool is_redirect;
  };

  // Value to use when the InfoBar has no icon to show.
  static const int kNoIconID;

  // Called when the InfoBar that owns this delegate is being destroyed.  At
  // this point nothing is visible onscreen.
  virtual ~InfoBarDelegate();

  virtual InfoBarAutomationType GetInfoBarAutomationType() const;

  // Returns true if the supplied |delegate| is equal to this one. Equality is
  // left to the implementation to define. This function is called by the
  // InfoBarManager when determining whether or not a delegate should be
  // added because a matching one already exists. If this function returns true,
  // the InfoBarManager will not add the new delegate because it considers
  // one to already be present.
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;

  // Returns true if the InfoBar should be closed automatically after the page
  // is navigated. By default this returns true if the navigation is to a new
  // page (not including reloads).  Subclasses wishing to change this behavior
  // can override either this function or ShouldExpireInternal(), depending on
  // what level of control they need.
  virtual bool ShouldExpire(const NavigationDetails& details) const;

  // Called when the user clicks on the close button to dismiss the infobar.
  virtual void InfoBarDismissed();

  // Return the resource ID of the icon to be shown for this InfoBar.  If the
  // value is equal to |kNoIconID|, no icon is shown.
  virtual int GetIconID() const;

  // Returns the type of the infobar.  The type determines the appearance (such
  // as background color) of the infobar.
  virtual Type GetInfoBarType() const;

  // Type-checking downcast routines:
  virtual AutoLoginInfoBarDelegate* AsAutoLoginInfoBarDelegate();
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate();
  virtual ExtensionInfoBarDelegate* AsExtensionInfoBarDelegate();
  virtual InsecureContentInfoBarDelegate* AsInsecureContentInfoBarDelegate();
  virtual MediaStreamInfoBarDelegate* AsMediaStreamInfoBarDelegate();
  virtual PopupBlockedInfoBarDelegate* AsPopupBlockedInfoBarDelegate();
  virtual RegisterProtocolHandlerInfoBarDelegate*
      AsRegisterProtocolHandlerInfoBarDelegate();
  virtual ScreenCaptureInfoBarDelegate* AsScreenCaptureInfoBarDelegate();
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
  virtual translate::TranslateInfoBarDelegate* AsTranslateInfoBarDelegate();

  void set_infobar(InfoBar* infobar) { infobar_ = infobar; }

  // Store the unique id for the active entry, to be used later upon navigation
  // to determine if this InfoBarDelegate should be expired.
  void StoreActiveEntryUniqueID();

  // Return the icon to be shown for this InfoBar. If the returned Image is
  // empty, no icon is shown.
  virtual gfx::Image GetIcon() const;

 protected:
  InfoBarDelegate();

  // Returns true if the navigation is to a new URL or a reload occured.
  virtual bool ShouldExpireInternal(const NavigationDetails& details) const;

  int contents_unique_id() const { return contents_unique_id_; }
  InfoBar* infobar() { return infobar_; }

 private:
  // The unique id of the active NavigationEntry of the WebContents that we were
  // opened for. Used to help expire on navigations.
  int contents_unique_id_;

  // The InfoBar associated with us.
  InfoBar* infobar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarDelegate);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
