// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/base/window_open_disposition.h"

class ConfirmInfoBarDelegate;
class HungRendererInfoBarDelegate;
class InsecureContentInfoBarDelegate;
class NativeAppInfoBarDelegate;
class PermissionInfobarDelegate;
class PopupBlockedInfoBarDelegate;
class RegisterProtocolHandlerInfoBarDelegate;
class ScreenCaptureInfoBarDelegate;
class ThemeInstalledInfoBarDelegate;
class ThreeDAPIInfoBarDelegate;

#if defined(OS_ANDROID)
class MediaStreamInfoBarDelegateAndroid;
class MediaThrottleInfoBarDelegate;
#endif

namespace translate {
class TranslateInfoBarDelegate;
}

namespace gfx {
class Image;
enum class VectorIconId;
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
    bool is_reload;
    bool is_redirect;
  };

  // Value to use when the InfoBar has no icon to show.
  static const int kNoIconID;

  // Called when the InfoBar that owns this delegate is being destroyed.  At
  // this point nothing is visible onscreen.
  virtual ~InfoBarDelegate();

  // Returns the type of the infobar.  The type determines the appearance (such
  // as background color) of the infobar.
  virtual Type GetInfoBarType() const;

  virtual InfoBarAutomationType GetInfoBarAutomationType() const;

  // Returns the resource ID of the icon to be shown for this InfoBar.  If the
  // value is equal to |kNoIconID|, GetIcon() will not show an icon by default.
  virtual int GetIconId() const;

  // Returns the vector icon identifier to be shown for this InfoBar. This will
  // take precedence over GetIconId() (although typically only one of the two
  // should be defined for any given infobar).
  virtual gfx::VectorIconId GetVectorIconId() const;

  // Returns the icon to be shown for this InfoBar. If the returned Image is
  // empty, no icon is shown.
  //
  // Most subclasses should not override this; override GetIconId() instead
  // unless the infobar needs to show an image from somewhere other than the
  // resource bundle as its icon.
  virtual gfx::Image GetIcon() const;

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

  // Type-checking downcast routines:
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate();
  virtual HungRendererInfoBarDelegate* AsHungRendererInfoBarDelegate();
  virtual InsecureContentInfoBarDelegate* AsInsecureContentInfoBarDelegate();
  virtual NativeAppInfoBarDelegate* AsNativeAppInfoBarDelegate();
  virtual PermissionInfobarDelegate* AsPermissionInfobarDelegate();
  virtual PopupBlockedInfoBarDelegate* AsPopupBlockedInfoBarDelegate();
  virtual RegisterProtocolHandlerInfoBarDelegate*
      AsRegisterProtocolHandlerInfoBarDelegate();
  virtual ScreenCaptureInfoBarDelegate* AsScreenCaptureInfoBarDelegate();
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
  virtual ThreeDAPIInfoBarDelegate* AsThreeDAPIInfoBarDelegate();
  virtual translate::TranslateInfoBarDelegate* AsTranslateInfoBarDelegate();
#if defined(OS_ANDROID)
  virtual MediaStreamInfoBarDelegateAndroid*
  AsMediaStreamInfoBarDelegateAndroid();
  virtual MediaThrottleInfoBarDelegate* AsMediaThrottleInfoBarDelegate();
#endif

  void set_infobar(InfoBar* infobar) { infobar_ = infobar; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

 protected:
  InfoBarDelegate();

  InfoBar* infobar() { return infobar_; }

 private:
  // The InfoBar associated with us.
  InfoBar* infobar_;

  // The ID of the active navigation entry at the time we became owned.
  int nav_entry_id_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarDelegate);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
